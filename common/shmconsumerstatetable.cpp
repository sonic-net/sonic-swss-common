#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"
#include "redisapi.h"
#include "shmShmConsumerStateTable.h"

using namespace std;

using namespace boost::interprocess;

#define MQ_RESPONSE_BUFFER_SIZE (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10
#define MQ_POLL_TIMEOUT (1000)

namespace swss {

ShmConsumerStateTable::ShmConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize, int pri)
    : Selectable(pri)
{
    m_queueName = db->getDbName() + "_" + tableName;
    
    try
    {
        m_queue = std::make_shared<message_queue>(open_or_create,
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

    m_runThread = true;
    m_mqPollThread = std::make_shared<std::thread>(&ShmProducerStateTable::updateTableThreadFunction, this);
    
}

ShmConsumerStateTable::~ShmConsumerStateTable()
{
    m_runThread = false;
}

void ShmConsumerStateTable::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");
    std::vector<uint8_t> buffer;
    buffer.resize(MQ_RESPONSE_BUFFER_SIZE);

    while (m_runThread)
    {
        unsigned int priority;
        message_queue::size_type recvd_size;

        try
        {
            bool received = m_messageQueue->timed_receive(buffer.data(),
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
                        recvd_size);
            }

            m_buffer.at(recvd_size) = 0; // make sure that we end string with zero before parse
            //m_queue.push((char*)m_buffer.data());

            m_selectableEvent.notify(); // will release epoll
        }
        catch (const interprocess_exception& e)
        {
            SWSS_LOG_ERROR("message queue %s timed receive failed: %s", m_queueName.c_str(), e.what());
            break;
        }
    }

    SWSS_LOG_NOTICE("end");
}

}
