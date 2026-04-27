#include "componentstats.h"

#include "dbconnector.h"
#include "table.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <unordered_map>
#include <utility>

namespace swss {

namespace {

std::string toUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

// Registry of created instances keyed by component name, so callers that
// forget to hold onto the shared_ptr still get the same singleton back.
std::mutex& registryMutex()
{
    static std::mutex m;
    return m;
}

std::map<std::string, std::weak_ptr<ComponentStats>>& registry()
{
    static std::map<std::string, std::weak_ptr<ComponentStats>> r;
    return r;
}

} // namespace

std::shared_ptr<ComponentStats> ComponentStats::create(
    const std::string& componentName,
    const std::string& dbName,
    uint32_t intervalSec)
{
    const std::string key = toUpper(componentName);
    const uint32_t effectiveInterval = intervalSec == 0 ? 1 : intervalSec;

    std::lock_guard<std::mutex> lock(registryMutex());
    auto& slot = registry()[key];
    if (auto existing = slot.lock())
    {
        // First call wins. Warn if a later caller passes different params
        // so the inconsistency is at least visible in syslog.
        if (existing->m_dbName != dbName ||
            existing->m_intervalSec != effectiveInterval)
        {
            SWSS_LOG_WARN("ComponentStats[%s]: ignoring later create() with "
                          "db=%s interval=%us; live instance uses db=%s "
                          "interval=%us",
                          key.c_str(),
                          dbName.c_str(), effectiveInterval,
                          existing->m_dbName.c_str(), existing->m_intervalSec);
        }
        return existing;
    }

    // std::make_shared cannot access the private constructor; use new + wrap.
    std::shared_ptr<ComponentStats> inst(
        new ComponentStats(key, dbName, effectiveInterval));
    slot = inst;
    return inst;
}

ComponentStats::ComponentStats(const std::string& componentName,
                               const std::string& dbName,
                               uint32_t intervalSec)
    : m_component(componentName)
    , m_tableName(componentName + "_STATS")
    , m_dbName(dbName)
    , m_intervalSec(intervalSec == 0 ? 1 : intervalSec)
{
    SWSS_LOG_ENTER();
    m_thread = std::make_unique<std::thread>(&ComponentStats::writerThread, this);
    SWSS_LOG_NOTICE("ComponentStats[%s] initialized (db=%s, interval=%us)",
                    m_component.c_str(), m_dbName.c_str(), m_intervalSec);
}

ComponentStats::~ComponentStats()
{
    stop();
}

void ComponentStats::stop()
{
    if (!m_running.exchange(false))
    {
        return; // already stopped
    }
    m_cv.notify_all();
    if (m_thread && m_thread->joinable())
    {
        m_thread->join();
    }
    SWSS_LOG_NOTICE("ComponentStats[%s] stopped", m_component.c_str());
}

void ComponentStats::setEnabled(bool on)
{
    m_enabled.store(on, std::memory_order_relaxed);
}

bool ComponentStats::isEnabled() const
{
    return m_enabled.load(std::memory_order_relaxed);
}

ComponentStats::EntityStats&
ComponentStats::getOrCreateEntity(const std::string& entity)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entities.find(entity);
    if (it == m_entities.end())
    {
        it = m_entities.emplace(std::piecewise_construct,
                                std::forward_as_tuple(entity),
                                std::forward_as_tuple()).first;
    }
    // std::map never invalidates references to existing elements on insert,
    // so returning this reference after the lock is released is safe.
    return it->second;
}

ComponentStats::Counter&
ComponentStats::getOrCreateCounter(EntityStats& e, const std::string& metric)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = e.metrics.find(metric);
    if (it == e.metrics.end())
    {
        // std::map::emplace constructs Counter in place; no move required.
        it = e.metrics.emplace(std::piecewise_construct,
                               std::forward_as_tuple(metric),
                               std::forward_as_tuple()).first;
    }
    return it->second;
}

void ComponentStats::increment(const std::string& entity,
                               const std::string& metric,
                               uint64_t n)
{
    if (!isEnabled() || n == 0)
    {
        return;
    }
    auto& e = getOrCreateEntity(entity);
    auto& c = getOrCreateCounter(e, metric);
    c.value.fetch_add(n, std::memory_order_relaxed);
    // Release ordering pairs with the acquire in writerThread() so that counter
    // writes become visible before the version bump.
    e.version.fetch_add(1, std::memory_order_release);
}

void ComponentStats::setValue(const std::string& entity,
                              const std::string& metric,
                              uint64_t value)
{
    if (!isEnabled())
    {
        return;
    }
    auto& e = getOrCreateEntity(entity);
    auto& c = getOrCreateCounter(e, metric);
    c.value.store(value, std::memory_order_relaxed);
    e.version.fetch_add(1, std::memory_order_release);
}

uint64_t ComponentStats::get(const std::string& entity, const std::string& metric)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto eit = m_entities.find(entity);
    if (eit == m_entities.end()) return 0;
    auto mit = eit->second.metrics.find(metric);
    if (mit == eit->second.metrics.end()) return 0;
    return mit->second.value.load(std::memory_order_relaxed);
}

ComponentStats::CounterSnapshot
ComponentStats::getAll(const std::string& entity)
{
    CounterSnapshot snap;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto eit = m_entities.find(entity);
    if (eit == m_entities.end()) return snap;
    for (const auto& kv : eit->second.metrics)
    {
        snap[kv.first] = kv.second.value.load(std::memory_order_relaxed);
    }
    return snap;
}

void ComponentStats::writerThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("ComponentStats[%s] writer thread started",
                    m_component.c_str());

    // Defer the DB connection: early-startup containers may call create()
    // before Redis is reachable. Retry on failure with a bounded back-off.
    while (m_running.load(std::memory_order_relaxed))
    {
        try
        {
            m_db = std::make_shared<DBConnector>(m_dbName, 0);
            m_table = std::make_unique<Table>(m_db.get(), m_tableName);
            break;
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_WARN("ComponentStats[%s]: DB connect failed: %s. Retrying…",
                          m_component.c_str(), e.what());
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::seconds(m_intervalSec),
                          [this]{ return !m_running.load(std::memory_order_relaxed); });
        }
    }

    std::unordered_map<std::string, uint64_t> lastVersions;

    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::seconds(m_intervalSec),
                          [this]{ return !m_running.load(std::memory_order_relaxed); });
            if (!m_running.load(std::memory_order_relaxed))
            {
                break;
            }
        }

        // Defensive: the connect-retry loop above only exits via `break`
        // (m_table set) or via m_running becoming false (in which case the
        // m_running check at the top of this loop already broke out). This
        // line therefore should not normally be reached.
        if (!m_table) continue;

        std::vector<std::string> keys;
        std::vector<std::vector<FieldValueTuple>> values;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& entry : m_entities)
            {
                const std::string& name = entry.first;
                const EntityStats& stats = entry.second;

                // Acquire pairs with release in increment()/setValue(),
                // guaranteeing we see all counter updates performed before
                // the version bump we're about to read.
                uint64_t curVer = stats.version.load(std::memory_order_acquire);

                auto vit = lastVersions.find(name);
                if (vit != lastVersions.end() && vit->second == curVer)
                {
                    continue; // no change since last flush
                }
                lastVersions[name] = curVer;

                std::vector<FieldValueTuple> row;
                row.reserve(stats.metrics.size());
                for (const auto& mkv : stats.metrics)
                {
                    row.emplace_back(
                        mkv.first,
                        std::to_string(mkv.second.value.load(std::memory_order_relaxed)));
                }
                keys.push_back(name);
                values.push_back(std::move(row));
            }
        }

        // Write outside the lock so that increment() / setValue() stay
        // responsive while Redis round-trips are in flight.
        for (size_t i = 0; i < keys.size(); ++i)
        {
            try
            {
                m_table->set(keys[i], values[i]);
            }
            catch (const std::exception& e)
            {
                SWSS_LOG_WARN("ComponentStats[%s]: write failed for %s: %s",
                              m_component.c_str(), keys[i].c_str(), e.what());
            }
        }
    }

    SWSS_LOG_NOTICE("ComponentStats[%s] writer thread stopped",
                    m_component.c_str());
}

} // namespace swss
