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
#include "zmqconsumerstatetable.h"
#include "json.h"

#include <zmq.h>

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

    SWSS_LOG_DEBUG("ZmqConsumerStateTable ctor tableName: %s, endpoint: %s", tableName.c_str(), endpoint.c_str());
}

ZmqConsumerStateTable::~ZmqConsumerStateTable()
{
    m_runThread = false;
    m_mqPollThread->join();
}

void ZmqConsumerStateTable::handleReceivedData(const char *json, Table& table)
{
    auto ptr = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *ptr;
    auto& values = kfvFieldsValues(kco);
    swss::JSon::readJson(json, values);

    // set key and OP
    swss::FieldValueTuple fvt = values.at(0);
    kfvKey(kco) = fvField(fvt);
    kfvOp(kco) = fvValue(fvt);
    values.erase(values.begin());
    
    if (kfvOp(kco) == SET_COMMAND)
    {
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
        std::lock_guard<std::mutex> lock(m_dataQueueMutex);
        m_dataQueue.push(ptr);
    }

    m_selectableEvent.notify(); // will release epoll
}

void ZmqConsumerStateTable::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");
    std::vector<uint8_t> buffer;
    buffer.resize(MQ_RESPONSE_BUFFER_SIZE);
    
    // Follow same logic in ConsumerStateTable: every received data will write to 'table'.
    DBConnector db(m_db->getDbName(), 0, true);
    Table table(&db, getTableName());

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
        rc = zmq_recv(socket, buffer.data(), MQ_RESPONSE_BUFFER_SIZE, ZMQ_DONTWAIT);
        if (rc < 0)
        {
            SWSS_LOG_DEBUG("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_errno());
            if (zmq_errno() == EINTR || zmq_errno() == EAGAIN)
            {
                sleep(0);
                continue;
            }
            else
            {
                SWSS_LOG_THROW("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_errno());
            }
        }

        if (rc >= MQ_RESPONSE_BUFFER_SIZE)
        {
            SWSS_LOG_THROW("zmq_recv message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                    MQ_RESPONSE_BUFFER_SIZE,
                    rc);
        }

        buffer.at(rc) = 0; // make sure that we end string with zero before parse
        SWSS_LOG_DEBUG("zmq received %d bytes", rc);

        // deserialize and write to redis:
        handleReceivedData((const char*)buffer.data(), table);
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
        // size() and empty() are not thread safe
        std::lock_guard<std::mutex> lock(m_dataQueueMutex);
        if (m_dataQueue.empty())
        {
            return;
        }

        // For new data append to m_dataQueue during pops, will not be include in result.
        count = m_dataQueue.size();
    }

    vkco.clear();
    for (size_t ie = 0; ie < count; ie++)
    {
        auto& kco = *(m_dataQueue.front());
        vkco.push_back(std::move(kco));

        {
            std::lock_guard<std::mutex> lock(m_dataQueueMutex);
            m_dataQueue.pop();
        }
    }
}

}
