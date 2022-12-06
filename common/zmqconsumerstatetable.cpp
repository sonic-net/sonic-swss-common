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

void ZmqConsumerStateTable::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");
    std::vector<uint8_t> buffer;
    buffer.resize(MQ_RESPONSE_BUFFER_SIZE);

    // Producer/Consumer state table are n:m mapping, so need use PUSH/PUUL pattern http://api.zeromq.org/master:zmq-socket
    void* context = zmq_ctx_new();;
    void* socket = zmq_socket(context, ZMQ_PULL);
    int rc = zmq_bind(socket, m_endpoint.c_str());
    if (rc != 0)
    {
        SWSS_LOG_THROW("zmq_bind failed on endpoint: %s, zmqerrno: %d",
                m_endpoint.c_str(),
                zmq_errno());
    }

    SWSS_LOG_NOTICE("bind to zmq endpoint: %s", m_endpoint.c_str());

    while (m_runThread)
    {
        // receive message
        rc = zmq_recv(socket, buffer.data(), MQ_RESPONSE_BUFFER_SIZE, ZMQ_DONTWAIT);
        if (rc < 0)
        {
            //cout << "zmq_recv: " << rc << " errno:" << zmq_errno() << endl;
            if (zmq_errno() == EINTR || zmq_errno() == EAGAIN)
            {
                SWSS_LOG_DEBUG("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_errno());
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

        {
            std::lock_guard<std::mutex> lock(m_dataQueueMutex);
            m_dataQueue.push((char*)buffer.data());
        }

        m_selectableEvent.notify(); // will release epoll
    }

    zmq_close(socket);
    zmq_ctx_destroy(context);

    SWSS_LOG_NOTICE("end");
}

/* Get multiple pop elements */
void ZmqConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string& /*prefix*/)
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
