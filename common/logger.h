#ifndef SWSS_COMMON_LOGGER_H
#define SWSS_COMMON_LOGGER_H

#include <string>
#include <chrono>
#include <atomic>
#include <map>
#include <memory>
#include <thread>

namespace swss {

#define SWSS_LOG_ERROR(MSG, ...)       swss::Logger::getInstance().write(swss::Logger::SWSS_ERROR,  ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)
#define SWSS_LOG_WARN(MSG, ...)        swss::Logger::getInstance().write(swss::Logger::SWSS_WARN,   ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)
#define SWSS_LOG_NOTICE(MSG, ...)      swss::Logger::getInstance().write(swss::Logger::SWSS_NOTICE, ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)
#define SWSS_LOG_INFO(MSG, ...)        swss::Logger::getInstance().write(swss::Logger::SWSS_INFO,   ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)
#define SWSS_LOG_DEBUG(MSG, ...)       swss::Logger::getInstance().write(swss::Logger::SWSS_DEBUG,  ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)

#define SWSS_LOG_ENTER()               swss::Logger::ScopeLogger logger ## __LINE__ (__LINE__, __FUNCTION__)
#define SWSS_LOG_TIMER(msg, ...)       swss::Logger::ScopeTimer scopetimer ## __LINE__ (__LINE__, __FUNCTION__, msg, ##__VA_ARGS__)

#define SWSS_LOG_THROW(MSG, ...)       swss::Logger::getInstance().wthrow(swss::Logger::SWSS_ERROR,  ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)

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

    typedef std::map<std::string, Priority> PriorityStringMap;
    typedef std::function<void (std::string component, std::string prioStr)> PriorityChangeNotify;
    typedef std::map<std::string, PriorityChangeNotify> PriorityChangeObserver;
    static const PriorityStringMap priorityStringMap;

    static Logger &getInstance();
    static void setMinPrio(Priority prio);
    static Priority getMinPrio();
    static void linkToDb(const std::string dbName, const PriorityChangeNotify& notify, const std::string& defPrio);
    // Must be called after all linkToDb to start select from DB
    static void linkToDbNative(const std::string dbName);
    void write(Priority prio, const char *fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 3, 4)))
#endif
    ;

    void wthrow(Priority prio, const char *fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 3, 4)))
        __attribute__ ((noreturn))
#endif
    ;

    static std::string priorityToString(Priority prio);

    class ScopeLogger
    {
        public:

        ScopeLogger(int line, const char *fun);
        ~ScopeLogger();

        private:
            const int m_line;
            const char *m_fun;
    };

    class ScopeTimer
    {
        typedef std::chrono::duration<double, std::ratio<1>> second_t;

        public:

            ScopeTimer(int line, const char* fun, const char *msg, ...);
            ~ScopeTimer();

        private:

            const int m_line;
            const char *m_fun;

            std::string m_msg;

            std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    };

private:
    Logger(){};
    ~Logger();
    Logger(const Logger&);
    Logger &operator=(const Logger&);

    static void swssNotify(std::string component, std::string prioStr);
    void prioThread();

    PriorityChangeObserver m_priorityChangeObservers;
    std::map<std::string, std::string> m_currentPrios;
    std::atomic<Priority> m_minPrio = { SWSS_NOTICE };
    std::unique_ptr<std::thread> m_prioThread;
};

}

#endif /* SWSS_COMMON_LOGGER_H */
