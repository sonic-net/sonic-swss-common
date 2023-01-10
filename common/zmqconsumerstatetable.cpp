#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include <zmq.h>
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

ZmqConsumerStateTable::ZmqConsumerStateTable(DBConnector *db, const std::string &tableName, const std::string& endpoint, int popBatchSize, int pri)
    : Selectable(pri)
    , m_db(db)
    , m_tableName(tableName)
    , m_endpoint(endpoint)
{
    m_tableSeparator = TableBase::gettableSeparator(db->getDbId());
    m_runThread = true;
    m_mqPollThread = std::make_shared<std::thread>(&ZmqConsumerStateTable::mqPollThread, this);
    m_dbUpdateThread = std::make_shared<std::thread>(&ZmqConsumerStateTable::dbUpdateThread, this);

    SWSS_LOG_DEBUG("ZmqConsumerStateTable ctor tableName: %s, endpoint: %s", tableName.c_str(), endpoint.c_str());
}

ZmqConsumerStateTable::~ZmqConsumerStateTable()
{
    m_runThread = false;

    // notify db update thread exit
    m_dbUpdateDataNotifyCv.notify_all();

    m_mqPollThread->join();
    m_dbUpdateThread->join();
}

std::shared_ptr<KeyOpFieldsValuesTuple> ZmqConsumerStateTable::deserializeReceivedData(const char* buffer, const size_t size)
{
    auto ptr = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *ptr;
    auto& values = kfvFieldsValues(kco);
    BinarySerializer::deserializeBuffer(buffer, size, values);

    // set key and OP
    swss::FieldValueTuple fvt = values.at(0);
    kfvKey(kco) = fvField(fvt);
    kfvOp(kco) = fvValue(fvt);
    values.erase(values.begin());

    return ptr;
}

void ZmqConsumerStateTable::handleReceivedData(const char* buffer, const size_t size)
{
    auto pkco = deserializeReceivedData(buffer, size);
    {
        std::lock_guard<std::mutex> lock(m_mqPoolDataQueueMutex);
        m_mqPoolDataQueue.push(pkco);
    }

    m_selectableEvent.notify(); // will release epoll

    pkco = deserializeReceivedData(buffer, size);
    {
        std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
        m_dbUpdateDataQueue.push(pkco);
    }

    m_dbUpdateDataNotifyCv.notify_all();
}

void ZmqConsumerStateTable::dbUpdateThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("dbUpdateThread begin");

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
                table.set(kfvKey(kco), values);
            }
            else if (kfvOp(kco) == DEL_COMMAND)
            {
                table.del(kfvKey(kco));
            }
            else
            {
                SWSS_LOG_ERROR("zmq endpoint: %s, receive unknown operation: %s", m_endpoint.c_str(), kfvOp(kco).c_str());
            }

            {
                std::lock_guard<std::mutex> lock(m_dbUpdateDataQueueMutex);
                m_dbUpdateDataQueue.pop();
            }
        }
    }
}

void ZmqConsumerStateTable::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");
    std::vector<char> buffer;
    buffer.resize(MQ_RESPONSE_MAX_COUNT);

    // Producer/Consumer state table are n:1 mapping, so need use PUSH/PULL pattern http://api.zeromq.org/master:zmq-socket
    void* context = zmq_ctx_new();;
    void* socket = zmq_socket(context, ZMQ_PULL);
    int rc = zmq_bind(socket, m_endpoint.c_str());
    if (rc != 0)
    {
        SWSS_LOG_THROW("zmq_bind failed on endpoint: %s, zmqerrno: %d",
                m_endpoint.c_str(),
                zmq_errno());
    }

    // zmq_poll will use less CPU
    zmq_pollitem_t poll_item[1];
    poll_item[0].fd = 0;
    poll_item[0].socket = socket;
    poll_item[0].events = ZMQ_POLLIN;
    poll_item[0].revents = 0;

    SWSS_LOG_NOTICE("bind to zmq endpoint: %s", m_endpoint.c_str());
    while (m_runThread)
    {
        // receive message
        rc = zmq_poll(poll_item, 1, 1);
        if (rc == 0 || !(poll_item[0].revents & ZMQ_POLLIN))
        {
            // timeout or other event
            SWSS_LOG_DEBUG("zmq_poll timeout or invalied event rc: %d, revents: %d", rc, poll_item[0].revents);
            continue;
        }

        // receive message
        rc = zmq_recv(socket, buffer.data(), MQ_RESPONSE_MAX_COUNT, ZMQ_DONTWAIT);
        if (rc < 0)
        {
            int zmq_err = zmq_errno();
            SWSS_LOG_DEBUG("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_err);
            if (zmq_err == EINTR || zmq_err == EAGAIN)
            {
                continue;
            }
            else
            {
                SWSS_LOG_THROW("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_err);
            }
        }

        if (rc >= MQ_RESPONSE_MAX_COUNT)
        {
            SWSS_LOG_THROW("zmq_recv message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                    MQ_RESPONSE_MAX_COUNT,
                    rc);
        }

        buffer.at(rc) = 0; // make sure that we end string with zero before parse
        SWSS_LOG_DEBUG("zmq received %d bytes", rc);

        // deserialize and write to redis:
        handleReceivedData(buffer.data(), rc);
    }

    zmq_close(socket);
    zmq_ctx_destroy(context);

    SWSS_LOG_NOTICE("mqPollThread end");
}

/* Get multiple pop elements */
void ZmqConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string& /*prefix*/)
{
    size_t count;
    {
        // size() is not thread safe
        std::lock_guard<std::mutex> lock(m_mqPoolDataQueueMutex);

        // For new data append to m_dataQueue during pops, will not be include in result.
        count = m_mqPoolDataQueue.size();
        if (!count)
        {
            return;
        }
    }

    vkco.clear();
    for (size_t ie = 0; ie < count; ie++)
    {
        auto& kco = *(m_mqPoolDataQueue.front());
        vkco.push_back(std::move(kco));

        {
            std::lock_guard<std::mutex> lock(m_mqPoolDataQueueMutex);
            m_mqPoolDataQueue.pop();
        }
    }
}

}
