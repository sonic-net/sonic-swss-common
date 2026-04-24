#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>

namespace swss {

class DBConnector;
class Table;

/**
 * ComponentStats — a lightweight, container-agnostic counter library.
 *
 * Each instance writes counters to a Redis hash of the form:
 *     <COMPONENT>_STATS:<entity>
 * where each field is a user-defined metric name (e.g. SET, DEL, GET, ERROR…).
 *
 * Design goals:
 *   - Lock-free hot path: increment() / setValue() use std::atomic operations.
 *   - Deferred DB connection: the DBConnector is created inside the writer
 *     thread rather than the constructor, so early-startup callers (singleton
 *     initialization before Redis is ready) do not crash.
 *   - Version-based dirty tracking: the writer thread only pushes entities
 *     whose counters changed since the last flush.
 *   - Stable references: std::map is used instead of unordered_map so that
 *     references returned by getOrCreateEntity() remain valid after later
 *     inserts (unordered_map can rehash and invalidate them).
 *   - Clean shutdown: the destructor uses condition_variable::notify_all() to
 *     wake the writer thread immediately instead of waiting up to interval.
 *
 * Thread safety: all public methods are safe to call from any thread.
 */
class ComponentStats
{
public:
    /// Snapshot of counters for a single entity (used for diagnostics / tests).
    using CounterSnapshot = std::map<std::string, uint64_t>;

    /**
     * Create a ComponentStats instance.
     *
     * @param componentName  Used as the Redis key prefix, upper-cased.
     *                       For example, "swss" produces keys like
     *                       "SWSS_STATS:<entity>".
     * @param dbName         Redis logical DB name (default: "COUNTERS_DB").
     * @param intervalSec    Writer thread flush period in seconds.
     * @return A shared instance. Repeated calls with the same componentName
     *         return the same underlying singleton.
     */
    static std::shared_ptr<ComponentStats> create(
        const std::string& componentName,
        const std::string& dbName = "COUNTERS_DB",
        uint32_t intervalSec = 1);

    ~ComponentStats();

    /// Add n to the named metric on the given entity. Creates the entity on
    /// first use. Safe to call concurrently from many threads.
    void increment(const std::string& entity,
                   const std::string& metric,
                   uint64_t n = 1);

    /// Overwrite the metric with an absolute value. Useful for gauge-style
    /// stats (e.g. queue depth) that are not monotonically increasing.
    void setValue(const std::string& entity,
                  const std::string& metric,
                  uint64_t value);

    /// Read a single metric. Returns 0 if either entity or metric are unknown.
    uint64_t get(const std::string& entity, const std::string& metric);

    /// Snapshot all metrics for an entity. Returns empty map if unknown.
    CounterSnapshot getAll(const std::string& entity);

    /// Globally enable / disable recording. Hot-path calls become no-ops when
    /// disabled; the writer thread keeps running but has nothing to flush.
    /// Intended to be wired to a CONFIG_DB knob if runtime control is desired.
    void setEnabled(bool on);
    bool isEnabled() const;

    /// Stop the writer thread. Called automatically by the destructor.
    void stop();

    // Non-copyable / non-movable (singleton-ish usage)
    ComponentStats(const ComponentStats&) = delete;
    ComponentStats& operator=(const ComponentStats&) = delete;

private:
    struct Counter
    {
        std::atomic<uint64_t> value{0};
        Counter() = default;
        Counter(const Counter&) = delete;
        Counter& operator=(const Counter&) = delete;
    };

    struct EntityStats
    {
        // Map of metric name -> counter. std::map keeps references stable.
        std::map<std::string, std::unique_ptr<Counter>> metrics;
        // Incremented whenever any counter on this entity changes.
        std::atomic<uint64_t> version{0};

        EntityStats() = default;
        EntityStats(const EntityStats&) = delete;
        EntityStats& operator=(const EntityStats&) = delete;
    };

    ComponentStats(const std::string& componentName,
                   const std::string& dbName,
                   uint32_t intervalSec);

    // Returns a stable reference. Safe to keep after the lock is released
    // because std::map does not invalidate element references on insert.
    EntityStats& getOrCreateEntity(const std::string& entity);
    Counter&     getOrCreateCounter(EntityStats& e, const std::string& metric);

    void writerThread();

    const std::string m_component;   // already upper-cased
    const std::string m_tableName;   // "<COMPONENT>_STATS"
    const std::string m_dbName;
    const uint32_t    m_intervalSec;

    std::atomic<bool> m_enabled{true};
    std::atomic<bool> m_running{true};

    mutable std::mutex      m_mutex;       // guards m_entities structure
    std::condition_variable m_cv;          // wakes writer on stop()
    std::map<std::string, EntityStats> m_entities;

    std::shared_ptr<DBConnector>  m_db;    // created in writerThread
    std::unique_ptr<Table>        m_table;

    std::unique_ptr<std::thread>  m_thread;
};

} // namespace swss
