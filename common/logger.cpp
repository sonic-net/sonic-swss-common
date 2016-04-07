#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

using namespace swss;

Logger::Priority Logger::m_minPrio = Logger::SWSS_NOTICE;

Logger &Logger::getInstance()
{
    static Logger m_logger;
    return m_logger;
}

void Logger::setMinPrio(Priority prio)
{
    m_minPrio = prio;
}

void Logger::write(Priority prio, const char *fmt, ...)
{

    if (prio > m_minPrio)
        return;

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
