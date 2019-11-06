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
#define assert(exp) DebugFramework::customAssert(exp, DebugFramework::AssertAction::UNSPECIFIED, __PRETTY_FUNCTION__, __LINE__)
#endif

#define SWSS_DEBUG_ASSERT_LOG(exp)     DebugFramework::customAssert(exp, DebugFramework::AssertAction::SYSLOG, __PRETTY_FUNCTION__, __LINE__)
#define SWSS_DEBUG_ASSERT_DUMP(exp)    DebugFramework::customAssert(exp, DebugFramework::AssertAction::DUMP, __PRETTY_FUNCTION__, __LINE__)
#define SWSS_DEBUG_ASSERT_BTRACE(exp)  DebugFramework::customAssert(exp, DebugFramework::AssertAction::BTRACE, __PRETTY_FUNCTION__, __LINE__)
#define SWSS_DEBUG_ASSERT_ABORT(exp)   DebugFramework::customAssert(exp, DebugFramework::AssertAction::ABORT, __PRETTY_FUNCTION__, __LINE__)


namespace swss {

#define SWSS_DEBUG_PRINT          swss::DebugFramework::getInstance().dumpWrite
#define SWSS_DEBUG_PRINT_BEGIN    swss::DebugFramework::getInstance().dumpBegin
#define SWSS_DEBUG_PRINT_END      swss::DebugFramework::getInstance().dumpEnd

typedef void (*DumpInfoFunc)(std::string, KeyOpFieldsValuesTuple);

class DebugFramework 
{
public:
    static DebugFramework  &getInstance();       /* To have only one instance aka singleton */

    /* This function will internally create a runnable thread, 
     * create selectable to receive events and waits on events
     */
    static void linkWithFramework(const std::string &componentName, const DumpInfoFunc funcPtr);
 
    /* This function will not create any thread and calling component
     * need to handle triggers via Redis to invoke DumpInfoFunc 
     */
    static void linkWithFrameworkNoThread(const std::string &componentName); 

    /* This function will create FieldValueTuples and publish to Redis 
     * args: comma seperated strings opaque to framework passed to components
     */
    static void invokeTrigger(const std::string componentName, std::string args);  

    void dumpBegin(const std::string &component);
    void dumpEnd(const std::string &component);
    void dumpWrite(const std::string &component, const char *fmt, ...)
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
    static void setAssertAction(const std::string val);
    static void customAssert(bool exp, AssertAction act, const char *func, const unsigned line);

private: 
    DebugFramework(); 
    ~DebugFramework();
    DebugFramework(const DebugFramework&);
    DebugFramework &operator=(const DebugFramework&);

    static void listenDoneEvents(const std::string componentName);
    static void relayTrigger(const std::string &componentName, KeyOpFieldsValuesTuple args);  
    static void updateRegisteredComponents(const std::string &component);

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
    void setTarget(const std::string &t);
    PostAction getPostAction();
    void setPostAction(const std::string &p);
    
    std::vector<std::string> m_registeredComps;
    std::map<std::string, std::fstream*> m_compFds;
    std::unique_ptr<Table>  m_configParams;
    
    TargetLocation     m_targetLocation;
    PostAction         m_postAction;

#ifndef NO_RET_TYPE
#define NO_RET_TYPE [[noreturn]]
#endif
    NO_RET_TYPE static void runnableThread(const std::string componentName, const DumpInfoFunc funcPtr);   

    AssertAction getAssertAction();
};

}

#endif  /* SWSS_COMMON_DEBUGFRAMEWORK_H */
