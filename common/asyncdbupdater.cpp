#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include <pthread.h>
#include "asyncdbupdater.h"
#include "dbconnector.h"
#include "redisselect.h"
#include "redisapi.h"
#include "table.h"

using namespace std;

namespace swss {

AsyncDBUpdater::AsyncDBUpdater(DBConnector *db, const std::string &tableName)
    : m_db(db)
    , m_tableName(tableName)
{
    m_runThread = true;
    m_dbUpdateThread = std::make_shared<std::thread>(&AsyncDBUpdater::dbUpdateThread, this);

    SWSS_LOG_DEBUG("AsyncDBUpdater ctor tableName: %s", tableName.c_str());
}

AsyncDBUpdater::~AsyncDBUpdater()
{
    m_runThread = false;

    // notify db update thread exit
    m_dbUpdateDataNotifyCv.notify_all();
    m_dbUpdateThread->join();
}

void AsyncDBUpdater::update(std::shared_ptr<KeyOpFieldsValuesTuple> pkco)
{
    {
        std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
        m_dbUpdateDataQueue.push(pkco);
    }

    m_dbUpdateDataNotifyCv.notify_all();
}

void AsyncDBUpdater::dbUpdateThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("dbUpdateThread begin");

    // Different schedule policy has different min priority 
    pthread_attr_t attr;
    int policy;
    pthread_attr_getschedpolicy(&attr, &policy);
    int min_priority = sched_get_priority_min(policy);
    // Use min priority will block poll thread 
    pthread_setschedprio(pthread_self(), min_priority + 1);

    // Follow same logic in ConsumerStateTable: every received data will write to 'table'.
    DBConnector db(m_db->getDbName(), 0, true);
    Table table(&db, m_tableName);
    std::mutex cvMutex;
    std::unique_lock<std::mutex> cvLock(cvMutex);

    while (m_runThread)
    {
        m_dbUpdateDataNotifyCv.wait(cvLock);

        size_t count;
        {
            // size() is not thread safe
            std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
 
            // For new data append to m_dataQueue during pops, will not be include in result.
            count = m_dbUpdateDataQueue.size();
            if (!count)
            {
                continue;
            }

        }

        for (size_t ie = 0; ie < count; ie++)
        {
            auto& kco = *(m_dbUpdateDataQueue.front());

            if (kfvOp(kco) == SET_COMMAND)
            {
                auto& values = kfvFieldsValues(kco);

                // Delete entry before Table::set(), because Table::set() does not remove the no longer existed fields from entry.
                table.del(kfvKey(kco));
                table.set(kfvKey(kco), values);
            }
            else if (kfvOp(kco) == DEL_COMMAND)
            {
                table.del(kfvKey(kco));
            }
            else
            {
                SWSS_LOG_ERROR("db: %s, table: %s receive unknown operation: %s", m_db->getDbName().c_str(), m_tableName.c_str(), kfvOp(kco).c_str());
            }

            {
                std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
                m_dbUpdateDataQueue.pop();
            }
        }
    }
}

}
