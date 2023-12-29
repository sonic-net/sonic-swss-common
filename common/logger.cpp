#include "logger.h"

#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "schema.h"
#include "select.h"
#include "dbconnector.h"
#include "subscriberstatetable.h"
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

Logger::~Logger()
{
    terminateSettingThread();
}

void Logger::terminateSettingThread()
{
    // can't be executed under mutex, since it can cause deadlock

    if (m_settingThread)
    {
        m_stopEvent->notify();

        m_settingThread->join();

        m_settingThread = nullptr;
    }
}

void Logger::restartSettingThread()
{
    terminateSettingThread();

    m_stopEvent.reset(new SelectableEvent(0));

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

    if (priorityStringMap.find(prioStr) == priorityStringMap.end())
    {
        SWSS_LOG_ERROR("Invalid loglevel. Setting to NOTICE. %s", prioStr.c_str());
        logger.m_minPrio = SWSS_NOTICE;
    }
    else
    {
        SWSS_LOG_DEBUG("Changing logger minPrio to %s", prioStr.c_str());
        logger.m_minPrio = priorityStringMap.at(prioStr);
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

    if (outputStringMap.find(outputStr) == outputStringMap.end())
    {
        SWSS_LOG_ERROR("Invalid logoutput. Setting to SYSLOG. %s", outputStr.c_str());
        logger.m_output = SWSS_SYSLOG;
    }
    else
    {
        logger.m_output = outputStringMap.at(outputStr);
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

    // Initialize internal DB with observer
    logger.m_settingChangeObservers.insert(std::make_pair(dbName, std::make_pair(prioNotify, outputNotify)));

    DBConnector db("CONFIG_DB", 0);
    swss::Table table(&db, CFG_LOGGER_TABLE_NAME);
    SWSS_LOG_DEBUG("Component %s register to logger", dbName.c_str());

    std::string prio, output;
    bool doUpdate = false;
    if(!table.hget(dbName, DAEMON_LOGLEVEL, prio))
    {
        prio = defPrio;
        doUpdate = true;
    }
    if(!table.hget(dbName, DAEMON_LOGOUTPUT, output))
    {
        output = defOutput;
        doUpdate = true;
    }

    if (doUpdate)
    {
        FieldValueTuple fvLevel(DAEMON_LOGLEVEL, prio);
        FieldValueTuple fvOutput(DAEMON_LOGOUTPUT, output);
        std::vector<FieldValueTuple>fieldValues = { fvLevel, fvOutput };
        SWSS_LOG_DEBUG("Set %s loglevel to %s", dbName.c_str() , prio.c_str());
        table.set(dbName, fieldValues);
    }

    logger.m_currentPrios.set(dbName, prio);
    logger.m_currentOutputs.set(dbName, output);

    prioNotify(dbName, prio);
    outputNotify(dbName, output);
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

        if (op != SET_COMMAND || !m_settingChangeObservers.contains(key))
        {
            continue;
        }

        const auto& values = kfvFieldsValues(koValues);

        for (auto& i : values)
        {
            auto& field = fvField(i);
            auto& value = fvValue(i);

            if ((field == DAEMON_LOGLEVEL) && (value != m_currentPrios.get(key)))
            {
                m_currentPrios.set(key, value);
                m_settingChangeObservers.get(key).first(key, value);
            }
            else if ((field == DAEMON_LOGOUTPUT) && (value != m_currentOutputs.get(key)))
            {
                m_currentOutputs.set(key, value);
                m_settingChangeObservers.get(key).second(key, value);
            }
        }
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
