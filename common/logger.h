#ifndef SWSS_COMMON_LOGGER_H
#define SWSS_COMMON_LOGGER_H

namespace swss {

#define SWSS_LOG_ERROR(MSG, ...)        Logger::getInstance().write(Logger::SWSS_ERROR, MSG, ##__VA_ARGS__)
#define SWSS_LOG_WARN(MSG, ...)         Logger::getInstance().write(Logger::SWSS_WARN, MSG, ##__VA_ARGS__)
#define SWSS_LOG_NOTICE(MSG, ...)       Logger::getInstance().write(Logger::SWSS_NOTICE, MSG, ##__VA_ARGS__)
#define SWSS_LOG_INFO(MSG, ...)         Logger::getInstance().write(Logger::SWSS_INFO, MSG, ##__VA_ARGS__)
#define SWSS_LOG_DEBUG(MSG, ...)        Logger::getInstance().write(Logger::SWSS_DEBUG, MSG, ##__VA_ARGS__)

class Logger
{
public:
    enum Priority
    {
        SWSS_EMERG,
        SWSS_ALERT,
        SWSS_CRIT,
        SWSS_ERROR,
        SWSS_WARN,
        SWSS_NOTICE,
        SWSS_INFO,
        SWSS_DEBUG
    };

    static Logger &getInstance();
    static void setMinPrio(Priority prio);
    void write(Priority prio, const char *fmt, ...);
private:
    Logger() {};
    Logger(const Logger&);
    Logger &operator=(const Logger&);

    static Priority m_minPrio;
};

}

#endif /* SWSS_COMMON_LOGGER_H */
