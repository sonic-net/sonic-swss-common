#include "debugframework.h"

#include <stdio.h>
#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <list>
#include "syslog.h"
#include "schema.h"
#include "select.h"
#include "dbconnector.h"
#include "redisclient.h"
#include "producerstatetable.h"
#include "subscriberstatetable.h"
#include "notificationconsumer.h"
#include "notificationproducer.h"

#define ALL_COMPONENTS  "all"
#define ASSERT_FILE "/var/log/assert_btrace.log"
#define BTRACE_BUF_SIZE 100

namespace swss {


DebugFramework &DebugFramework::getInstance()
{
    static DebugFramework m_debugframework;
    return m_debugframework;
}

DebugFramework::DebugFramework() {
    DBConnector db(CONFIG_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    m_configParams = std::unique_ptr<Table>(new Table(&db, APP_DEBUGFM_CONFIG_TABLE_NAME));
}

DebugFramework::~DebugFramework() {
}

void DebugFramework::linkWithFrameworkNoThread(const std::string &componentName)
{
    SWSS_LOG_ENTER();
    updateRegisteredComponents(componentName);
}

void DebugFramework::linkWithFramework(const std::string &componentName, const DumpInfoFunc funcPtr)
{
    SWSS_LOG_ENTER();
    if (funcPtr == NULL)
    {
        /* log error and return */
        SWSS_LOG_ERROR("Invalid arguments passed to debug framework");
        return;
    }
    
    // Add the component to the registry
    updateRegisteredComponents(componentName);
    
    // Now create a thread and let the rest be handled there
    std::thread t(DebugFramework::runnableThread, componentName, funcPtr);
    t.detach();
}

void DebugFramework::updateRegisteredComponents(const std::string &component)
{
    DBConnector db(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);

    Table table(&db, APP_DEBUG_RCOMPONENT_TABLE_NAME);
    FieldValueTuple fv("THREAD", "noOp");
    std::vector<FieldValueTuple>fieldValues = { fv };
    table.set(component, fieldValues);
}

NO_RET_TYPE void DebugFramework::runnableThread(const std::string &componentName, const DumpInfoFunc funcPtr)
{
    SWSS_LOG_ENTER();
    Select s;
    DBConnector dumpDb(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    DBConnector doneDb(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    NotificationProducer p(&doneDb, APP_DEBUG_COMP_DONE_TABLE_NAME);
    NotificationConsumer  dumpTrigger(&dumpDb, APP_DEBUG_COMPONENT_TABLE_NAME);

    s.addSelectable(&dumpTrigger);

    while (true)
    {
        Selectable *temp = nullptr;
        int ret = s.select(&temp, -1);

        if ( (ret == Select::ERROR) || (ret == Select::TIMEOUT))
        {
            SWSS_LOG_DEBUG("%s select error %s", __PRETTY_FUNCTION__, strerror(errno));
            continue;
        }

        if (temp == (Selectable *)&dumpTrigger)
        {
            /* match component name and invoke dump */    
            std::deque<KeyOpFieldsValuesTuple> entries;
            dumpTrigger.pops(entries);

            for (auto entry: entries)
            {
                std::string key = kfvKey(entry);
                if (key == componentName)
                {
                    FieldValueTuple fvResp("RESPONSE", "SUCCESS");
                    try{
                        (*funcPtr)(componentName, entry);
                    }catch(...){
                        SWSS_LOG_ERROR("%s dump routine failed.", componentName.c_str());
                        fvResp.second = "FAILURE";
                        std::vector<FieldValueTuple>fieldValues = { fvResp };
                        p.send(componentName, componentName, fieldValues);

                        throw "Registered dump routine returned error";
                    }

                    /* Inform done with same key */
                    std::vector<FieldValueTuple>fieldValues = { fvResp };
                    p.send(componentName, componentName, fieldValues);
                }
            }
        }
    }  // end while loop
}

void DebugFramework::invokeTrigger(const std::string &componentName, std::string argList)
{
    SWSS_LOG_ENTER();
    SWSS_LOG_DEBUG("DebugFramework invokeTrigger called for %s with args %s", componentName.c_str(), argList.c_str());

    auto& dbgfm = getInstance();

    //parse and construct KeyOpFieldsValuesTuple
    KeyOpFieldsValuesTuple kco;
    std::vector<FieldValueTuple> attrs;

    std::stringstream ss(argList);
    std::string pair;
    while(std::getline(ss, pair, ';'))
    {
        std::string f,v;
        long unsigned int pos = pair.find(':');
        if (pos == std::string::npos)
        {
            continue;
        }

        f = pair.substr(0, pos);
        v = pair.substr(pos+1);
        
        FieldValueTuple fvp(f,v);

        //seperate out args for framework consumption
        if ( f == "TARGET")
        {
            dbgfm.setTarget(v);
            continue;
        }

        if ( f == "ACTION")
        {
            dbgfm.setPostAction(v);
            continue;
        }
        attrs.push_back(fvp);
    }

    kfvFieldsValues(kco) = attrs;
    kfvKey(kco) = componentName;
    kfvOp(kco) = "SET";

    relayTrigger(componentName, kco);
}

void DebugFramework::relayTrigger(const std::string &componentName, KeyOpFieldsValuesTuple args)
{
    SWSS_LOG_ENTER();
    DBConnector db(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    Table table(&db, APP_DEBUG_RCOMPONENT_TABLE_NAME);

    DBConnector dumpDb(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    NotificationProducer producer(&dumpDb, APP_DEBUG_COMPONENT_TABLE_NAME);

    std::vector<std::string> components;
    std::vector<std::string> keys;
    table.getKeys(keys);

    if (componentName == ALL_COMPONENTS)
    {
        for (const auto &key: keys)
        {
            SWSS_LOG_DEBUG("DebugFramework sending notification for %s", key.c_str());
            producer.send(key, key, kfvFieldsValues(args));
        }
    }
    else
    {
        for (const auto &key: keys)
        {
            if (componentName == key)
            {
                SWSS_LOG_DEBUG("DebugFramework sending notification for %s", key.c_str());
                producer.send(key, key, kfvFieldsValues(args));
                break;
            }
        }
    }
    // Now create a thread and wait for done notifications
    std::thread t(DebugFramework::listenDoneEvents, componentName);
    t.detach();
}

void DebugFramework::listenDoneEvents(const std::string &componentName)
{
    SWSS_LOG_ENTER();
    Select sel;
    DBConnector db(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    Table table(&db, APP_DEBUG_RCOMPONENT_TABLE_NAME);

    DBConnector doneDb(APPL_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
    NotificationConsumer doneIndication(&doneDb, APP_DEBUG_COMP_DONE_TABLE_NAME);

    sel.addSelectable(&doneIndication);

    std::vector<std::string> components;
    std::vector<std::string> keys;
    table.getKeys(keys);

    if (componentName == ALL_COMPONENTS)
    {
        for (const auto &key: keys)
        {
            components.push_back(key);
        }
    }
    else
    {
        for (const auto &key: keys)
        {
            if (componentName == key)
            {
                components.push_back(key);
                break;
            }
        }
    }
    // Now create a thread and wait for done notifications
    //  Wait for done notifications from component(s) 
    while (!components.empty())
    {
        Selectable *temp = nullptr;
        int ret = sel.select(&temp, 6000);

        if ( (ret == Select::ERROR) || (ret == Select::TIMEOUT))
        {
            SWSS_LOG_ERROR("%s select error %s", __PRETTY_FUNCTION__, strerror(errno));
            break;
        }

        if (temp == (Selectable *)&doneIndication)
        {
            std::vector<FieldValueTuple> entries;
            std::string op, data;
            doneIndication.pop(op, data, entries);

            SWSS_LOG_DEBUG("DebugFramework done indication  from %s", data.c_str());

            for (unsigned i = 0; i != components.size(); ++i)
            {
                if (data == components[i])
                {
                    components.erase(components.begin()+i);
                    break;
                }
            }
        }
    } // end while
    
    //trigger the post action
    SWSS_LOG_DEBUG("DebugFramework triggered post action script");

    //performPost(); TBD 
}

void DebugFramework::dumpBegin(const std::string &component)
{
    auto& dbgfm = getInstance();
    DebugFramework::TargetLocation t = dbgfm.getTarget();

    if (t == SWSS_FILE)
    {
        // open a file in append mode and keep it open
        std::fstream *outfile;
        std::string options("/var/log/" + component + "_debug.log");
        try{
            outfile = new std::fstream();
            outfile->open(options, std::fstream::out | std::fstream::app);
        }catch(...){
            std::string s("SWSS_SYSLOG");
            dbgfm.setTarget(s);
        }
        
        // hold on to the fd
        dbgfm.m_compFds.insert(std::make_pair(component, outfile)); 
    }
}

void DebugFramework::dumpEnd(const std::string &component)
{
    auto& dbgfm = getInstance();
    DebugFramework::TargetLocation t = dbgfm.getTarget();
    
    if (t == SWSS_FILE)
    {
        auto itr = dbgfm.m_compFds.find(component);
        if (itr != dbgfm.m_compFds.end())
        {
            std::fstream *out = itr->second;
            out->close();
            delete out;
            dbgfm.m_compFds.erase(itr);
        }
    }
    return;
}

void DebugFramework::dumpWrite(const std::string &component, const char *fmt, ...)
{
    auto& dbgfm = getInstance();
    
    va_list ap;
    va_start(ap, fmt);

    if (dbgfm.getTarget() == SWSS_SYSLOG)
    {
        vsyslog(LOG_DEBUG, fmt, ap);
        va_end(ap);
    }
    else
    {
        char buffer[0x1000];
        vsnprintf(buffer, 0x1000, fmt, ap);
        va_end(ap);

        std::fstream *f;
        auto itr = dbgfm.m_compFds.find(component);
        if (itr != dbgfm.m_compFds.end())
        {
            f = itr->second;
            *f << buffer << std::endl;
        }
    }
}

DebugFramework::TargetLocation DebugFramework::getTarget()
{
    std::string key("TARGET"), val;
    m_configParams->hget(key, key, val);
    if (val == "SWSS_SYSLOG")
        return SWSS_SYSLOG;
    else
        return SWSS_FILE;
}

void DebugFramework::setTarget(const std::string &val)
{
    std::string key("TARGET");
    m_configParams->hset(key, key, val);
}

DebugFramework::PostAction DebugFramework::getPostAction()
{
    std::string key("ACTION"), val;
    m_configParams->hget(key, key, val);
    if (val == "UPLOAD")
        return UPLOAD;
    else
        return COMPRESS;
}

void DebugFramework::setPostAction(const std::string &val)
{
    std::string key("ACTION");
    m_configParams->hset(key, key, val);
}

void DebugFramework::setAssertAction(const std::string val)
{
    auto& dbgfm = getInstance();
    std::string key("ASSERTACT");
    dbgfm.m_configParams->hset(key, key, val);
}

DebugFramework::AssertAction DebugFramework::getAssertAction()
{
    std::string key("ASSERTACT"), val;
    m_configParams->hget(key, key, val);
    if (val == "SYSLOG")
        return SYSLOG;
    else if(val == "DUMP")
        return DUMP;
    else if(val == "BTRACE")
        return BTRACE;
    else
        return ABORT;
}

void DebugFramework::customAssert(bool exp, AssertAction act, const char *func, const unsigned line)
{
    SWSS_LOG_ENTER();
    auto& dbgfm = getInstance();
    if (exp)
    {
        return;
    }

    if (act == UNSPECIFIED)
    {
        act = dbgfm.getAssertAction();
    }

    // log to syslog irrespective of action
    syslog(LOG_CRIT, "Assertion failed at %s : %d\n", func, line);

    switch (act)
    {
        case SYSLOG:
            break;

        // break not added intentionally
        case DUMP:
            {
                std::string comp("all");
                std::string args("DETAIL:FULL;");
                invokeTrigger(comp, args);
            }

        case BTRACE:
            {
                std::FILE *fp = fopen(ASSERT_FILE, "w");
                void * buffer[BTRACE_BUF_SIZE];
                const int calls = backtrace(buffer, sizeof(buffer) / sizeof(void *));
                backtrace_symbols_fd(buffer, calls, fileno(fp));
            }
            break;

        case ABORT:
        default:
            {
                abort();
            }
            break;
    } // end switch
}

}
