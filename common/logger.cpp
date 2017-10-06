#include "logger.h"

#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <fstream>
#include <stdexcept>
#include <schema.h>
#include <select.h>
#include <dbconnector.h>
#include <redisclient.h>
#include <consumerstatetable.h>
#include <producerstatetable.h>

namespace swss {

Logger::~Logger() {
    if (m_prioThread) {
        m_prioThread->detach();
    }
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

void Logger::swssNotify(std::string component, std::string prioStr)
{
    auto& logger = getInstance();

    if (priorityStringMap.find(prioStr) == priorityStringMap.end())
    {
        SWSS_LOG_ERROR("Invalid loglevel. Setting to NOTICE.");
        logger.m_minPrio = SWSS_NOTICE;
    }

    logger.m_minPrio = priorityStringMap.at(prioStr);
}

void Logger::linkToDb(const std::string dbName, const PriorityChangeNotify& notify, const std::string& defPrio)
{
    auto& logger = getInstance();

    // Initialize internal DB with observer
    logger.m_priorityChangeObservers.insert(std::make_pair(dbName, notify));
    logger.m_currentPrios[dbName] = defPrio;

    DBConnector db(LOGLEVEL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    RedisClient redisClient(&db);
    auto keys = redisClient.keys("*");

    // Sync with external DB
    std::string key = dbName + ":" + dbName;
    // If entry not found in external DB - use default
    if (std::find(std::begin(keys), std::end(keys), key) == keys.end())
    {
        ProducerStateTable table(&db, dbName);
        FieldValueTuple fv(DAEMON_LOGLEVEL, defPrio);
        logger.m_currentPrios[dbName] = defPrio;
        std::vector<FieldValueTuple>fieldValues = { fv };
        table.set(dbName, fieldValues);
        notify(dbName, defPrio);
        return;
    }

    // There is entry in external DB - use it
    auto newPrio = redisClient.hget(key, DAEMON_LOGLEVEL);
    logger.m_currentPrios[dbName] = *newPrio;
    notify(dbName, *newPrio);
}

void Logger::linkToDbNative(const std::string dbName)
{
    auto& logger = getInstance();

    linkToDb(dbName, swssNotify, "NOTICE");
    logger.m_prioThread.reset(new std::thread(&Logger::prioThread, &logger));
}

Logger &Logger::getInstance()
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

[[ noreturn ]] void Logger::prioThread()
{
    Select select;
    DBConnector db(LOGLEVEL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    std::vector<std::shared_ptr<ConsumerStateTable>> selectables(m_priorityChangeObservers.size());

    for (const auto& i : m_priorityChangeObservers)
    {
        std::shared_ptr<ConsumerStateTable> table = std::make_shared<ConsumerStateTable>(&db, i.first);
        selectables.push_back(table);
        select.addSelectable(table.get());
    }

    while(true)
    {
        int fd = 0;
        Selectable *selectable = nullptr;

        int ret = select.select(&selectable, &fd);

        if (ret == Select::ERROR)
        {
            SWSS_LOG_NOTICE("%s select error %s", __PRETTY_FUNCTION__, strerror(errno));
            continue;
        }

        KeyOpFieldsValuesTuple koValues;
        dynamic_cast<ConsumerStateTable *>(selectable)->pop(koValues);
        std::string key = kfvKey(koValues), op = kfvOp(koValues);

        if ((op != SET_COMMAND) || (m_priorityChangeObservers.find(key) == m_priorityChangeObservers.end()))
        {
            continue;
        }

        auto values = kfvFieldsValues(koValues);
        for (const auto& i : values)
        {
            std::string field = fvField(i), value = fvValue(i);
            if ((field != DAEMON_LOGLEVEL) || (value == m_currentPrios[key]))
            {
                continue;
            }

            m_currentPrios[key] = value;
            m_priorityChangeObservers[key](key, value);
            break;
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
    vsyslog(prio, fmt, ap);
    va_end(ap);
}

void Logger::wthrow(Priority prio, const char *fmt, ...)
{
    char buffer[0x1000];

    va_list ap;
    va_start(ap, fmt);
    vsyslog(prio, fmt, ap);
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

};
