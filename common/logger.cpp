#include "logger.h"

#include <algorithm>
#include <set>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <pthread.h>
#include <new>
#include "schema.h"
#include "select.h"
#include "dbconnector.h"
#include "subscriberstatetable.h"
#include "table.h"
#include "producerstatetable.h"

using namespace swss;

#define MUTEX std::lock_guard<std::mutex> _lock(getInstance().m_mutex);

void swss::err_exit(const char *fn, int ln, int e, const char *fmt, ...)
{
    va_list ap;
    char buff[1024];
    size_t len;

    va_start(ap, fmt);
    snprintf(buff, sizeof(buff), "%s::%d err(%d/%s): ", fn, ln, e, strerror(e));
    len = strlen(buff);
    vsnprintf(buff+len, sizeof(buff)-len, fmt, ap);
    va_end(ap);
    SWSS_LOG_ERROR("Aborting: %s", buff);
    abort();
}

namespace
{
// volatile: a static that is only written is otherwise optimized away.
swss::Table* volatile s_stateTableAtExit = nullptr;
}

Logger::Logger()
{
    pthread_atfork(nullptr, nullptr, &Logger::atForkChild);
}

void Logger::atForkChild()
{
    // Only the forking thread exists in the child, so a lock another thread
    // held at the fork would never be released. The parent's STATE_DB socket
    // stays the parent's; the child opens its own on its next publish.
    auto& logger = getInstance();
    new (&logger.m_settingMutex) std::mutex();
    (void)logger.m_stateTable.release();

    // The setting thread is not in the child, and its stop event shares the
    // parent's eventfd: notifying it would stop the parent's thread. Released,
    // not reset, since destroying a joinable std::thread terminates. The
    // child has no setting thread until it calls restartLogger.
    (void)logger.m_settingThread.release();
    logger.m_stopEvent.reset();
}

Logger::~Logger()
{
    terminateSettingThread();

    // Left open at exit: closing it may log, and the logger is going away.
    // It stays reachable from s_stateTableAtExit, so it is not a leak.
    s_stateTableAtExit = m_stateTable.release();
}

void Logger::terminateSettingThread()
{
    // can't be executed under mutex, since it can cause deadlock

    if (m_settingThread)
    {
        m_stopEvent->notify();

        m_settingThread->join();

        m_settingThread = nullptr;

        m_stopEvent = nullptr;
    }
}

void Logger::restartSettingThread()
{
    terminateSettingThread();

    m_stopEvent = std::make_unique<SelectableEvent>(0);

    m_settingThread.reset(new std::thread(&Logger::settingThread, this));
}

const Logger::PriorityStringMap Logger::priorityStringMap = {
    { "EMERG", SWSS_EMERG },
    { "ALERT", SWSS_ALERT },
    { "CRIT", SWSS_CRIT },
    { "ERROR", SWSS_ERROR },
    { "WARN", SWSS_WARN },
    { "NOTICE", SWSS_NOTICE },
    { "INFO", SWSS_INFO },
    { "DEBUG", SWSS_DEBUG }
};

void Logger::swssPrioNotify(const std::string& component, const std::string& prioStr)
{
    auto& logger = getInstance();

    auto it = priorityStringMap.find(prioStr);
    if (it == priorityStringMap.end())
    {
        SWSS_LOG_ERROR("Invalid loglevel. Setting to NOTICE. %s", prioStr.c_str());
        logger.m_minPrio = SWSS_NOTICE;
    }
    else
    {
        SWSS_LOG_DEBUG("Changing logger minPrio to %s", prioStr.c_str());
        logger.m_minPrio = it->second;
    }
}

const Logger::OutputStringMap Logger::outputStringMap = {
    { "SYSLOG", SWSS_SYSLOG },
    { "STDOUT", SWSS_STDOUT },
    { "STDERR", SWSS_STDERR }
};

void Logger::swssOutputNotify(const std::string& component, const std::string& outputStr)
{
    auto& logger = getInstance();

    auto it = outputStringMap.find(outputStr);
    if (it == outputStringMap.end())
    {
        SWSS_LOG_ERROR("Invalid logoutput. Setting to SYSLOG. %s", outputStr.c_str());
        logger.m_output = SWSS_SYSLOG;
    }
    else
    {
        logger.m_output = it->second;
    }
}

void Logger::linkToDbWithOutput(
        const std::string& dbName,
        const PriorityChangeNotify& prioNotify,
        const std::string& defPrio,
        const OutputChangeNotify& outputNotify,
        const std::string& defOutput)
{
    auto& logger = getInstance();

    // The defaults are in place before the observer, so a setting thread
    // that is already running never sees the component without them.
    logger.m_defaultPrios.set(dbName, defPrio);
    logger.m_defaultOutputs.set(dbName, defOutput);

    // Initialize internal DB with observer
    logger.m_settingChangeObservers.insert(std::make_pair(dbName, std::make_pair(prioNotify, outputNotify)));

    DBConnector db("CONFIG_DB", 0);
    swss::Table table(&db, CFG_LOGGER_TABLE_NAME);
    SWSS_LOG_DEBUG("Component %s register to logger", dbName.c_str());

    // Held from the read to the publish, so a change the setting thread
    // applies meanwhile is either seen by the read or applied after it.
    std::lock_guard<std::mutex> lock(logger.m_settingMutex);

    // An absent field means the component's default, and it is not written back.
    std::string prio, output;
    if (!table.hget(dbName, DAEMON_LOGLEVEL, prio))
    {
        prio = defPrio;
    }
    if (!table.hget(dbName, DAEMON_LOGOUTPUT, output))
    {
        output = defOutput;
    }

    logger.m_currentPrios.set(dbName, prio);
    logger.m_currentOutputs.set(dbName, output);

    prioNotify(dbName, prio);
    outputNotify(dbName, output);

    publishSetting(dbName, prio, output);
}

void Logger::publishSetting(const std::string& key, const std::string& prio, const std::string& output)
{
    // The caller holds m_settingMutex.
    auto& logger = getInstance();

    // The STATE_DB row is what lists a component that has no CONFIG_DB row;
    // logging keeps working without it.
    try
    {
        // Created once per process and reused by every publish.
        if (!logger.m_stateTable)
        {
            DBConnector db("STATE_DB", 0);
            logger.m_stateTable = std::make_unique<swss::Table>(&db, STATE_LOGGER_TABLE_NAME);
        }
        logger.m_stateTable->set(key, { { DAEMON_LOGLEVEL, prio }, { DAEMON_LOGOUTPUT, output } });
    }
    catch (const std::exception& e)
    {
        // Reconnect on the next publish.
        logger.m_stateTable.reset();
        SWSS_LOG_WARN("Failed to publish %s log setting to STATE_DB: %s", key.c_str(), e.what());
    }
}

void Logger::linkToDb(const std::string& dbName, const PriorityChangeNotify& prioNotify, const std::string& defPrio)
{
    linkToDbWithOutput(dbName, prioNotify, defPrio, swssOutputNotify, "SYSLOG");
}

void Logger::linkToDbNative(const std::string& dbName, const char * defPrio)
{
    linkToDb(dbName, swssPrioNotify, defPrio);

    getInstance().restartSettingThread();
}

void Logger::restartLogger()
{
    getInstance().restartSettingThread();
}

Logger& Logger::getInstance()
{
    static Logger m_logger;
    return m_logger;
}

void Logger::setMinPrio(Priority prio)
{
    getInstance().m_minPrio = prio;
}

Logger::Priority Logger::getMinPrio()
{
    return getInstance().m_minPrio;
}

void Logger::settingThread()
{
    Select select;
    DBConnector db("CONFIG_DB", 0);
    std::map<std::string, std::shared_ptr<SubscriberStateTable>> selectables;
    auto table = std::make_shared<SubscriberStateTable>(&db, CFG_LOGGER_TABLE_NAME);
    selectables.emplace(CFG_LOGGER_TABLE_NAME, table);
    select.addSelectable(table.get());
    select.addSelectable(m_stopEvent.get());

    // The subscription replays only rows that exist, so a row deleted while
    // no subscription was live produces no DEL. Read after subscribing, so a
    // row that changes meanwhile is also seen as an event.
    std::vector<std::string> keys;
    swss::Table(&db, CFG_LOGGER_TABLE_NAME).getKeys(keys);
    std::set<std::string> present(keys.begin(), keys.end());
    for (const auto& observer : m_settingChangeObservers.getCopy())
    {
        const std::string& key = observer.first;
        if (present.find(key) == present.end())
        {
            applySetting(key, m_defaultPrios.get(key), m_defaultOutputs.get(key));
        }
    }

    while (1)
    {

        Selectable *selectable = nullptr;

        /* TODO Resolve latency caused by timeout at initialization. */
        int ret = select.select(&selectable, 1000); // Timeout if there is no data in 1000 ms.

        if (ret == Select::ERROR)
        {
            SWSS_LOG_NOTICE("%s select error %s", __PRETTY_FUNCTION__, strerror(errno));
            continue;
        }

        if (ret == Select::TIMEOUT)
        {
            SWSS_LOG_DEBUG("%s select timeout", __PRETTY_FUNCTION__);
            continue;
        }

        if (selectable == m_stopEvent.get())
        {
            break;
        }

        KeyOpFieldsValuesTuple koValues;
        SubscriberStateTable *subscriberStateTable = NULL;
        subscriberStateTable = dynamic_cast<SubscriberStateTable *>(selectable);
        if (subscriberStateTable == NULL)
        {
            SWSS_LOG_ERROR("dynamic_cast returned NULL");
            break;
        }
        subscriberStateTable->pop(koValues);
        std::string key = kfvKey(koValues), op = kfvOp(koValues);

        if (!m_settingChangeObservers.contains(key))
        {
            continue;
        }

        // A field the row lacks, or a deleted row, means the default.
        std::string prio = m_defaultPrios.get(key);
        std::string output = m_defaultOutputs.get(key);

        if (op == SET_COMMAND)
        {
            for (const auto& i : kfvFieldsValues(koValues))
            {
                if (fvField(i) == DAEMON_LOGLEVEL)
                {
                    prio = fvValue(i);
                }
                else if (fvField(i) == DAEMON_LOGOUTPUT)
                {
                    output = fvValue(i);
                }
            }
        }
        else if (op != DEL_COMMAND)
        {
            continue;
        }

        applySetting(key, prio, output);
    }
}

void Logger::applySetting(const std::string& key, const std::string& prio, const std::string& output)
{
    std::lock_guard<std::mutex> lock(m_settingMutex);
    bool changed = false;

    if (prio != m_currentPrios.get(key))
    {
        m_currentPrios.set(key, prio);
        m_settingChangeObservers.get(key).first(key, prio);
        changed = true;
    }

    if (output != m_currentOutputs.get(key))
    {
        m_currentOutputs.set(key, output);
        m_settingChangeObservers.get(key).second(key, output);
        changed = true;
    }

    if (changed)
    {
        publishSetting(key, prio, output);
    }
}

void Logger::write(Priority prio, const char *fmt, ...)
{
    if (prio > m_minPrio)
        return;

    // TODO
    // + add thread id using std::thread::id this_id = std::this_thread::get_id();
    // + add timestmap with millisecond resolution
    va_list ap;
    va_start(ap, fmt);

    if (m_output == SWSS_SYSLOG)
    {
        vsyslog(prio, fmt, ap);
    }
    else
    {
        std::stringstream ss;
        ss << std::setw(6) << std::right << priorityToString(prio);
        ss << fmt << std::endl;

        MUTEX;

        if (m_output == SWSS_STDOUT)
        {
            vprintf(ss.str().c_str(), ap);
        }
        else
        {
            vfprintf(stderr, ss.str().c_str(), ap);
        }
    }

    va_end(ap);
}

void Logger::wthrow(Priority prio, const char *fmt, ...)
{
    char buffer[0x1000];

    va_list ap;
    va_start(ap, fmt);

    if (m_output == SWSS_SYSLOG)
    {
        vsyslog(prio, fmt, ap);
    }
    else
    {
        std::stringstream ss;
        ss << std::setw(6) << std::right << priorityToString(prio);
        ss << fmt << std::endl;

        MUTEX;

        if (m_output == SWSS_STDOUT)
        {
            vprintf(ss.str().c_str(), ap);
        }
        else
        {
            vfprintf(stderr, ss.str().c_str(), ap);
        }
    }

    va_end(ap);

    va_start(ap, fmt);
    vsnprintf(buffer, 0x1000, fmt, ap);
    va_end(ap);

    throw std::runtime_error(buffer);
}

std::string Logger::priorityToString(Logger::Priority prio)
{
    for (const auto& i : priorityStringMap)
    {
        if (i.second == prio)
        {
            return i.first;
        }
    }

    return "UNKNOWN";
}

std::string Logger::outputToString(Logger::Output output)
{
    for (const auto& i : outputStringMap)
    {
        if (i.second == output)
        {
            return i.first;
        }
    }

    return "UNKNOWN";
}

Logger::ScopeLogger::ScopeLogger(int line, const char *fun) : m_line(line), m_fun(fun)
{
    swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG, ":> %s: enter", m_fun);
}

Logger::ScopeLogger::~ScopeLogger()
{
    swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG, ":< %s: exit", m_fun);
}

Logger::ScopeTimer::ScopeTimer(int line, const char *fun, const char *fmt, ...) :
    m_line(line),
    m_fun(fun)
{
    char buffer[0x1000];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 0x1000, fmt, ap);
    va_end(ap);

    m_msg = buffer;

    m_start = std::chrono::high_resolution_clock::now();
}

Logger::ScopeTimer::~ScopeTimer()
{
    auto end = std::chrono::high_resolution_clock::now();

    double duration = std::chrono::duration_cast<second_t>(end - m_start).count();

    Logger::getInstance().write(swss::Logger::SWSS_NOTICE, ":- %s: %s took %lf sec", m_fun, m_msg.c_str(), duration);
}
