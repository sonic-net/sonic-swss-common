#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include <zmq.h>
#include <pthread.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "selectableevent.h"
#include "redisselect.h"
#include "redisapi.h"
#include "zmqconsumerstatetable.h"
#include "binaryserializer.h"

using namespace std;

namespace swss {

ZmqConsumerStateTable::ZmqConsumerStateTable(DBConnector *db, const std::string &tableName, ZmqServer &zmqServer, int popBatchSize, int pri, bool dbPersistence)
    : Selectable(pri)
    , TableBase(tableName, TableBase::getTableSeparator(db->getDbId()))
    , m_db(db)
    , m_zmqServer(zmqServer)
{
    if (dbPersistence)
    {
        SWSS_LOG_DEBUG("Database persistence enabled, tableName: %s", tableName.c_str());
        m_runThread = true;
        m_dbUpdateThread = std::make_shared<std::thread>(&ZmqConsumerStateTable::dbUpdateThread, this);
    }
    else
    {
        SWSS_LOG_DEBUG("Database persistence disabled, tableName: %s", tableName.c_str());
        m_dbUpdateThread = nullptr;
    }

    m_zmqServer.registerMessageHandler(m_db->getDbName(), tableName, this);

    SWSS_LOG_DEBUG("ZmqConsumerStateTable ctor tableName: %s", tableName.c_str());
}

ZmqConsumerStateTable::~ZmqConsumerStateTable()
{
    if (m_dbUpdateThread != nullptr)
    {
        m_runThread = false;

        // notify db update thread exit
        m_dbUpdateDataNotifyCv.notify_all();
        m_dbUpdateThread->join();
    }
}

void ZmqConsumerStateTable::handleReceivedData(std::shared_ptr<KeyOpFieldsValuesTuple> pkco)
{
    std::shared_ptr<KeyOpFieldsValuesTuple> clone = nullptr;
    if (m_dbUpdateThread != nullptr)
    {
        // clone before put to received queue, because received data may change by consumer.
        clone = std::make_shared<KeyOpFieldsValuesTuple>(*pkco);
    }

    {
        std::lock_guard<std::mutex> lock(m_receivedQueueMutex);
        m_receivedOperationQueue.push(pkco);
    }

    m_selectableEvent.notify(); // will release epoll

    if (m_dbUpdateThread != nullptr)
    {
        {
            std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
            m_dbUpdateDataQueue.push(clone);
        }

        m_dbUpdateDataNotifyCv.notify_all();
    }
}

void ZmqConsumerStateTable::dbUpdateThread()
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
    Table table(&db, getTableName());
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
                SWSS_LOG_ERROR("zmq consumer table: %s, receive unknown operation: %s", getTableName().c_str(), kfvOp(kco).c_str());
            }

            {
                std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
                m_dbUpdateDataQueue.pop();
            }
        }
    }
}

/* Get multiple pop elements */
void ZmqConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string& /*prefix*/)
{
    size_t count;
    {
        // size() is not thread safe
        std::lock_guard<std::mutex> lock(m_receivedQueueMutex);

        // For new data append to m_dataQueue during pops, will not be include in result.
        count = m_receivedOperationQueue.size();
        if (!count)
        {
            return;
        }
    }

    vkco.clear();
    for (size_t ie = 0; ie < count; ie++)
    {
        auto& kco = *(m_receivedOperationQueue.front());
        vkco.push_back(std::move(kco));

        {
            std::lock_guard<std::mutex> lock(m_receivedQueueMutex);
            m_receivedOperationQueue.pop();
        }
    }
}

}
