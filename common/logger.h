#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <functional>

#include "concurrentmap.h"
#include "selectableevent.h"

namespace swss {

/*
 * The macros test the priority inline, before evaluating their arguments: a
 * message below the minimum priority costs a load and a branch, neither a call
 * nor the formatting of its arguments. SWSS_LOG_THROW always logs, as it throws.
 */

#define SWSS_LOG_PRIO(PRIO, MSG, ...)  (swss::Logger::isPrioEnabled(PRIO) ? \
        swss::Logger::getInstance().write(PRIO, ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__) : (void)0)

#define SWSS_LOG_ERROR(MSG, ...)       SWSS_LOG_PRIO(swss::Logger::SWSS_ERROR,  MSG, ##__VA_ARGS__)
#define SWSS_LOG_WARN(MSG, ...)        SWSS_LOG_PRIO(swss::Logger::SWSS_WARN,   MSG, ##__VA_ARGS__)
#define SWSS_LOG_NOTICE(MSG, ...)      SWSS_LOG_PRIO(swss::Logger::SWSS_NOTICE, MSG, ##__VA_ARGS__)
#define SWSS_LOG_INFO(MSG, ...)        SWSS_LOG_PRIO(swss::Logger::SWSS_INFO,   MSG, ##__VA_ARGS__)
#define SWSS_LOG_DEBUG(MSG, ...)       SWSS_LOG_PRIO(swss::Logger::SWSS_DEBUG,  MSG, ##__VA_ARGS__)

#define SWSS_LOG_ENTER()               swss::Logger::InlineScopeLogger logger ## __LINE__ (__FUNCTION__)
#define SWSS_LOG_TIMER(msg, ...)       swss::Logger::ScopeTimer scopetimer ## __LINE__ (__LINE__, __FUNCTION__, msg, ##__VA_ARGS__)

#define SWSS_LOG_THROW(MSG, ...)       swss::Logger::getInstance().wthrow(swss::Logger::SWSS_ERROR,  ":- %s: " MSG, __FUNCTION__, ##__VA_ARGS__)

static constexpr const char * const DAEMON_LOGLEVEL = "LOGLEVEL";
static constexpr const char * const DAEMON_LOGOUTPUT = "LOGOUTPUT";

void err_exit(const char *fn, int ln, int e, const char *fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 4, 5)))
        __attribute__ ((noreturn))
#endif
        ;

#define ABORT_IF_NOT(x, fmt, args...)                       \
    if (!(x)) {                                             \
        int e = errno;                                      \
        err_exit(__FUNCTION__, __LINE__, e, (fmt), ##args); \
    }

#ifdef __GNUC__
#    define ATTRIBUTE_NORTEURN __attribute__ ((noreturn))
#else
#    define ATTRIBUTE_NORTEURN
#endif

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
    static const PriorityStringMap priorityStringMap;
    typedef std::function<void (std::string component, std::string prioStr)> PriorityChangeNotify;

    enum Output
    {
        SWSS_SYSLOG,
        SWSS_STDOUT,
        SWSS_STDERR
    };

    typedef std::map<std::string, Output> OutputStringMap;
    static const OutputStringMap outputStringMap;
    typedef std::function<void (std::string component, std::string outputStr)> OutputChangeNotify;

    static Logger& getInstance();
    static void setMinPrio(Priority prio);
    static Priority getMinPrio();

    // Whether messages of prio are written: inline, without a call
    static bool isPrioEnabled(Priority prio)
    {
        return prio <= s_minPrio.load(std::memory_order_relaxed);
    }

    static void linkToDbWithOutput(
            const std::string& dbName,
            const PriorityChangeNotify& prioNotify,
            const std::string& defPrio,
            const OutputChangeNotify& outputNotify,
            const std::string& defOutput);

    static void linkToDb(const std::string& dbName, const PriorityChangeNotify& notify, const std::string& defPrio);
    // Must be called after all linkToDb to start select from DB
    static void linkToDbNative(const std::string& dbName, const char * defPrio="NOTICE");
    static void restartLogger();
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
    static std::string outputToString(Output output);
    static void swssOutputNotify(const std::string& component, const std::string& outputStr);

    /*
     * Logs entering and leaving a scope at DEBUG. Its constructor and
     * destructor stay out of line for binaries built against earlier headers,
     * whose SWSS_LOG_ENTER() constructs it.
     */
    class ScopeLogger
    {
        public:

        ScopeLogger(int line, const char *fun);
        ~ScopeLogger();

        private:
            const int m_line;
            const char *m_fun;
    };

    /*
     * ScopeLogger with the priority test inline, for SWSS_LOG_ENTER(): with
     * DEBUG off, entering and leaving the scope cost a load and a branch each.
     */
    class InlineScopeLogger
    {
        public:

        explicit InlineScopeLogger(const char *fun) : m_fun(fun)
        {
            if (isPrioEnabled(SWSS_DEBUG))
            {
                logScopeEnter(m_fun);
            }
        }

        ~InlineScopeLogger()
        {
            if (isPrioEnabled(SWSS_DEBUG))
            {
                logScopeExit(m_fun);
            }
        }

        private:
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
    Logger() = default;
    ~Logger();
    Logger(const Logger&);
    Logger& operator=(const Logger&);

    static void swssPrioNotify(const std::string& component, const std::string& prioStr);

    static void logScopeEnter(const char *fun);
    static void logScopeExit(const char *fun);

    void settingThread();
    void terminateSettingThread();
    void restartSettingThread();

    typedef ConcurrentMap<std::string, std::pair<PriorityChangeNotify, OutputChangeNotify>> LogSettingChangeObservers;

    LogSettingChangeObservers m_settingChangeObservers;
    ConcurrentMap<std::string, std::string> m_currentPrios;
    // the minimum priority written, static for isPrioEnabled to read it inline
    static std::atomic<Priority> s_minPrio;
    ConcurrentMap<std::string, std::string> m_currentOutputs;
    std::atomic<Output> m_output = { SWSS_SYSLOG };
    std::unique_ptr<std::thread> m_settingThread;
    std::mutex m_mutex;
    std::unique_ptr<SelectableEvent> m_stopEvent;
};

}
