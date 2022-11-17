#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "selectableevent.h"
#include "redisselect.h"
#include "redisapi.h"
#include "shmconsumerstatetable.h"
#include "json.h"

#include <boost/interprocess/ipc/message_queue.hpp>

using namespace std;

using namespace boost::interprocess;

namespace swss {

ShmConsumerStateTable::ShmConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize, int pri)
    : Selectable(pri)
    , m_db(db)
    , m_tableName(tableName)
{
    m_tableSeparator = TableBase::gettableSeparator(db->getDbId());
    m_queueName = GetQueueName(db->getDbName(), tableName);

    m_runThread = true;
    m_mqPollThread = std::make_shared<std::thread>(&ShmConsumerStateTable::mqPollThread, this);
    
}

ShmConsumerStateTable::~ShmConsumerStateTable()
{
    m_runThread = false;
    m_mqPollThread->join();
}

bool ShmConsumerStateTable::TryRemoveShmQueue(const std::string &queueName)
{
    try
    {
        message_queue::remove(queueName.c_str());
        return true;
    }
    catch (const interprocess_exception& e)
    {
        // message queue still use by some other process
        SWSS_LOG_NOTICE("Failed to close a using message queue '%s': %s", queueName.c_str(), e.what());
    }
    
    return false;
}

std::string ShmConsumerStateTable::GetQueueName(const std::string &dbName, const std::string &tableName)
{
    return dbName + "_" + tableName;
}

void ShmConsumerStateTable::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");
    std::vector<uint8_t> buffer;
    buffer.resize(MQ_RESPONSE_BUFFER_SIZE);
    
    std::shared_ptr<message_queue> msgQueue;
    
    try
    {
        msgQueue = std::make_shared<message_queue>(open_or_create,
                                                   m_queueName.c_str(),
                                                   MQ_SIZE,
                                                   MQ_RESPONSE_BUFFER_SIZE);
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("failed to open or create main message queue %s: %s",
                m_queueName.c_str(),
                e.what());
    }

    while (m_runThread)
    {
        unsigned int priority;
        message_queue::size_type recvd_size;

        try
        {
            bool received = msgQueue->timed_receive(buffer.data(),
                                                MQ_RESPONSE_BUFFER_SIZE,
                                                recvd_size,
                                                priority,
                                                boost::posix_time::ptime(microsec_clock::universal_time()) + boost::posix_time::milliseconds(MQ_POLL_TIMEOUT));

            if (m_runThread == false)
            {
                SWSS_LOG_NOTICE("ending pool thread, since run is false");
                break;
            }

            if (!received)
            {
                // Not receive message
                SWSS_LOG_DEBUG("message queue timed receive: no events, continue");
                continue;
            }

            if (recvd_size >= MQ_RESPONSE_BUFFER_SIZE)
            {
                SWSS_LOG_THROW("message queue received message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                        MQ_RESPONSE_BUFFER_SIZE,
                        (int)recvd_size);
            }

            buffer.at(recvd_size) = 0; // make sure that we end string with zero before parse

            {
                std::lock_guard<std::mutex> lock(m_dataQueueMutex);
                m_dataQueue.push((char*)buffer.data());
            }

            m_selectableEvent.notify(); // will release epoll
        }
        catch (const interprocess_exception& e)
        {
            SWSS_LOG_ERROR("message queue %s timed receive failed: %s", m_queueName.c_str(), e.what());
            break;
        }
    }

    TryRemoveShmQueue(m_queueName);

    SWSS_LOG_NOTICE("end");
}

/* Get multiple pop elements */
void ShmConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string& /*prefix*/)
{
    if (m_dataQueue.empty())
    {
        return;
    }
    
    // For new data append to m_dataQueue during pops, will not be include in result.
    auto count = m_dataQueue.size();
    vkco.clear();
    vkco.resize(count);
    for (size_t ie = 0; ie < count; ie++)
    {
        auto& kco = vkco[ie];
        auto& values = kfvFieldsValues(kco);
        values.clear();

        swss::JSon::readJson(m_dataQueue.front(), values);
        
        // set key and OP
        swss::FieldValueTuple fvt = values.at(0);
        kfvKey(kco) = fvField(fvt);
        kfvOp(kco) = fvValue(fvt);

        values.erase(values.begin());

        {
            std::lock_guard<std::mutex> lock(m_dataQueueMutex);
            m_dataQueue.pop();
        }
    }
}

}
