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
    if (popBatchSize > 0)
    {
        m_popBatchSize = (size_t)popBatchSize;
    }
    else
    {
        m_popBatchSize = DEFAULT_POP_BATCH_SIZE;
        SWSS_LOG_ERROR("Invalid pop batch size: Setting it to %d", DEFAULT_POP_BATCH_SIZE);
    }

    if (dbPersistence)
    {
        SWSS_LOG_DEBUG("Database persistence enabled, tableName: %s", tableName.c_str());
        m_asyncDBUpdater = std::make_unique<AsyncDBUpdater>(db, tableName);
    }
    else
    {
        SWSS_LOG_DEBUG("Database persistence disabled, tableName: %s", tableName.c_str());
        m_asyncDBUpdater = nullptr;
    }

    m_zmqServer.registerMessageHandler(m_db->getDbName(), tableName, this);

    SWSS_LOG_DEBUG("ZmqConsumerStateTable ctor tableName: %s", tableName.c_str());
}

void ZmqConsumerStateTable::handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos)
{
    for (auto kco : kcos)
    {
        std::shared_ptr<KeyOpFieldsValuesTuple> clone = nullptr;
        if (m_asyncDBUpdater != nullptr)
        {
            // clone before put to received queue, because received data may change by consumer.
            clone = std::make_shared<KeyOpFieldsValuesTuple>(*kco);
        }

        {
            std::lock_guard<std::mutex> lock(m_receivedQueueMutex);
            m_receivedOperationQueue.push(kco);
        }

        if (m_asyncDBUpdater != nullptr)
        {
            m_asyncDBUpdater->update(clone);
        }
    }
    m_selectableEvent.notify(); // will release epoll
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
    auto pop_limit = min(count, m_popBatchSize);
    for (size_t ie = 0; ie < pop_limit; ie++)
    {
        auto& kco = *(m_receivedOperationQueue.front());
        vkco.push_back(std::move(kco));

        {
            std::lock_guard<std::mutex> lock(m_receivedQueueMutex);
            m_receivedOperationQueue.pop();
        }
    }

    if (count > m_popBatchSize)
    {
        // Notify epoll to wake up and continue to pop.
        m_selectableEvent.notify();
    }
}

size_t ZmqConsumerStateTable::dbUpdaterQueueSize()
{
    if (m_asyncDBUpdater == nullptr)
    {
        throw system_error(make_error_code(errc::operation_not_supported),
                           "Database persistence is not enabled");
    }

    return m_asyncDBUpdater->queueSize();
}

}
