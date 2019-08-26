#ifndef SWSS_COMMON_DEBUGFRAMEWORK_H
#define SWSS_COMMON_DEBUGFRAMEWORK_H

#include <map>
#include <thread>
#include <assert.h>
#include <execinfo.h>
#include "dbconnector.h"
#include "table.h"


#ifdef assert
#undef assert
#define assert(exp) Debugframework::custom_assert(exp, Debugframework::AssertAction::UNSPECIFIED, __PRETTY_FUNCTION__, __LINE__)
#endif

#define SWSS_DEBUG_ASSERT_LOG(exp)     Debugframework::custom_assert(exp, Debugframework::AssertAction::SYSLOG, __PRETTY_FUNCTION__, __LINE__)
#define SWSS_DEBUG_ASSERT_DUMP(exp)    Debugframework::custom_assert(exp, Debugframework::AssertAction::DUMP, __PRETTY_FUNCTION__, __LINE__)
#define SWSS_DEBUG_ASSERT_BTRACE(exp)  Debugframework::custom_assert(exp, Debugframework::AssertAction::BTRACE, __PRETTY_FUNCTION__, __LINE__)
#define SWSS_DEBUG_ASSERT_ABORT(exp)   Debugframework::custom_assert(exp, Debugframework::AssertAction::ABORT, __PRETTY_FUNCTION__, __LINE__)


namespace swss {

#define SWSS_DEBUG_PRINT          swss::Debugframework::getInstance().dumpWrite
#define SWSS_DEBUG_PRINT_BEGIN    swss::Debugframework::getInstance().dumpBegin
#define SWSS_DEBUG_PRINT_END      swss::Debugframework::getInstance().dumpEnd

typedef void (*DumpInfoFunc)(std::string, KeyOpFieldsValuesTuple);

class Debugframework 
{
public:
    static Debugframework  &getInstance();       /* To have only one instance aka singleton */

    //typedef std::function< void (std::string componentName, KeyOpFieldsValuesTuple args)> DumpInfoFunc;

    /* This function will internally create a runnable thread, 
     * create selectable to receive events and waits on events
     */
    static void linkWithFramework(std::string &componentName, const DumpInfoFunc funcPtr);
 
    /* This function will not create any thread and calling component
     * need to handle triggers via Redis to invoke DumpInfoFunc 
     */
    static void linkWithFrameworkNoThread(std::string &componentName); 

    /* This function will create FieldValueTuples and publish to Redis 
     * args: comma seperated strings opaque to framework passed to components
     */
    static void invokeTrigger(std::string componentName, std::string args);  

    void dumpBegin(std::string &component);
    void dumpEnd(std::string &component);
    void dumpWrite(std::string &component, const char *fmt, ...)
#ifdef __GNUC__
        __attribute__ ((format (printf, 3, 4)))
#endif
    ;

    enum AssertAction
    {
        SYSLOG,
        DUMP,
        BTRACE,
        ABORT,
        UNSPECIFIED
    };
    static void setAssertAction(std::string val);
    static void custom_assert(bool exp, AssertAction act, const char *func, const unsigned line);

private: 
    Debugframework(); 
    ~Debugframework();
    Debugframework(const Debugframework&);
    Debugframework &operator=(const Debugframework&);

    static void listenDoneEvents(std::string componentName);
    static void relayTrigger(std::string &componentName, KeyOpFieldsValuesTuple args);  
    static void updateRegisteredComponents(std::string &component);

    enum PostAction
    {
        COMPRESS,
        UPLOAD
    };
    
    enum TargetLocation
    {
        SWSS_SYSLOG,
        SWSS_FILE
    };

    TargetLocation getTarget();
    void setTarget(std::string &t);
    PostAction getPostAction();
    void setPostAction(std::string &p);
    
    std::vector<std::string> m_registeredComps;
    std::map<std::string, std::fstream*> m_compFds;
    std::unique_ptr<Table>  m_configParams;
    
    TargetLocation     m_targetLocation;
    PostAction         m_postAction;

#ifndef NO_RET_TYPE
#define NO_RET_TYPE [[noreturn]]
#endif
    NO_RET_TYPE static void runnableThread(std::string componentName, const DumpInfoFunc funcPtr);   

    AssertAction getAssertAction();
};

}

#endif  /* SWSS_COMMON_DEBUGFRAMEWORK_H */
