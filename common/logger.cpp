#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <fstream>

namespace swss {

Logger::Priority Logger::m_minPrio = Logger::SWSS_NOTICE;

Logger::Logger()
{
    std::ifstream self("/proc/self/cmdline", std::ios::binary);

    m_self = "unknown";

    if (self.is_open())
    {
        self >> m_self;

        size_t found = m_self.find_last_of("/");

        if (found != std::string::npos)
        {
            m_self = m_self.substr(found + 1);
        }

        self.close();
    }
}

Logger &Logger::getInstance()
{
    static Logger m_logger;
    return m_logger;
}

void Logger::setMinPrio(Priority prio)
{
    m_minPrio = prio;
}

Logger::Priority Logger::getMinPrio()
{
    return m_minPrio;
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

Logger::ScopeLogger::ScopeLogger(int line, const char *fun) : m_line(line), m_fun(fun)
{
    swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG, ":> %s: enter", m_fun);
}

Logger::ScopeLogger::~ScopeLogger()
{
    swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG, ":< %s: exit", m_fun);
}

Logger::ScopeTimer::ScopeTimer(int line, const char *fun, std::string msg) :
    m_line(line),
    m_fun(fun),
    m_msg(msg)
{
    m_start = std::chrono::high_resolution_clock::now();
}

Logger::ScopeTimer::~ScopeTimer()
{
    auto end = std::chrono::high_resolution_clock::now();

    double duration = std::chrono::duration_cast<second_t>(end - m_start).count();

    Logger::getInstance().write(swss::Logger::SWSS_INFO, ":- %s: %s took %lf", m_fun, m_msg.c_str(), duration);
}

std::string Logger::priorityToString(Logger::Priority prio)
{
    switch(prio)
    {
    case swss::Logger::SWSS_EMERG:
        return "EMERG";

    case swss::Logger::SWSS_ALERT:
        return "ALERT";

    case swss::Logger::SWSS_CRIT:
        return "CRIT";

    case swss::Logger::SWSS_ERROR:
        return "ERROR";

    case swss::Logger::SWSS_WARN:
        return "WARN";

    case swss::Logger::SWSS_NOTICE:
        return "NOTICE";

    case swss::Logger::SWSS_INFO:
        return "INFO";

    case swss::Logger::SWSS_DEBUG:
        return "DEBUG";
    }

    SWSS_LOG_WARN("unable to parse prioriy %d as log priority, returning notice", prio);

    return "NOTICE";
}

Logger::Priority Logger::stringToPriority(const std::string level)
{
    if (level == "EMERG")
        return swss::Logger::SWSS_EMERG;

    if (level == "ALERT")
        return swss::Logger::SWSS_ALERT;

    if (level == "CRIT")
        return swss::Logger::SWSS_CRIT;

    if (level == "ERROR")
        return swss::Logger::SWSS_ERROR;

    if (level == "WARN")
        return swss::Logger::SWSS_WARN;

    if (level == "NOTICE")
        return swss::Logger::SWSS_NOTICE;

    if (level == "INFO")
        return swss::Logger::SWSS_INFO;

    if (level == "DEBUG")
        return swss::Logger::SWSS_DEBUG;

    SWSS_LOG_WARN("unable to parse %s as log priority, returning notice", level.c_str());

    return swss::Logger::SWSS_NOTICE;
}

};
