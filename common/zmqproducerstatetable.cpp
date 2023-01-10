#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <utility>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <zmq.h>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "redispipeline.h"
#include "zmqproducerstatetable.h"
#include "zmqconsumerstatetable.h"
#include "binaryserializer.h"

using namespace std;

namespace swss {

ZmqProducerStateTable::ZmqProducerStateTable(DBConnector *db, const string &tableName, const std::string& endpoint)
    : ProducerStateTable(db, tableName)
{
    initialize(endpoint);
}

ZmqProducerStateTable::ZmqProducerStateTable(RedisPipeline *pipeline, const string &tableName, const std::string& endpoint, bool buffered)
    : ProducerStateTable(pipeline, tableName, buffered)
{
    initialize(endpoint);
}

ZmqProducerStateTable::~ZmqProducerStateTable()
{
    if (m_socket)
    {
        int rc = zmq_close(m_socket);
        if (rc != 0)
        {
            SWSS_LOG_ERROR("failed to close zmq socket, zmqerrno: %d",
                    zmq_errno());
        }
    }

    if (m_context)
    {
        zmq_ctx_destroy(m_context);
    }
}
    
void ZmqProducerStateTable::initialize(const std::string& endpoint)
{
    m_connected = false;
    m_endpoint = endpoint;
    m_context = nullptr;
    m_socket = nullptr;
    m_sendbuffer.resize(MQ_RESPONSE_MAX_COUNT);

    connect(endpoint);
}
    
void ZmqProducerStateTable::connect(const std::string& endpoint)
{
    if (m_connected)
    {
        SWSS_LOG_DEBUG("Already connected to endpoint: %s", endpoint.c_str());
        return;
    }

    if (m_socket)
    {
        int rc = zmq_close(m_socket);
        if (rc != 0)
        {
            SWSS_LOG_ERROR("failed to close zmq socket, zmqerrno: %d", zmq_errno());
        }
    }

    if (m_context)
    {
        zmq_ctx_destroy(m_context);
    }
    
    // Producer/Consumer state table are n:1 mapping, so need use PUSH/PULL pattern http://api.zeromq.org/master:zmq-socket
    m_context = zmq_ctx_new();
    m_socket = zmq_socket(m_context, ZMQ_PUSH);
    
    // timeout all pending send package, so zmq will not block in dtor of this class: http://api.zeromq.org/master:zmq-setsockopt
    int linger = 0;
    zmq_setsockopt(m_socket, ZMQ_LINGER, &linger, sizeof(linger));

    SWSS_LOG_NOTICE("connect to zmq endpoint: %s", endpoint.c_str());
    int rc = zmq_connect(m_socket, endpoint.c_str());
    if (rc != 0)
    {
        m_connected = false;
        SWSS_LOG_ERROR("failed to connect to zmq endpoint %s, zmqerrno: %d",
                endpoint.c_str(),
                zmq_errno());
    }

    m_connected = true;
}

void ZmqProducerStateTable::set(
                    const string &key,
                    const vector<FieldValueTuple> &values,
                    const string &op /*= SET_COMMAND*/,
                    const string &prefix)
{
    sendMsg(key, values, op);
}

void ZmqProducerStateTable::del(
                    const string &key,
                    const string &op /*= DEL_COMMAND*/,
                    const string &prefix)
{
    sendMsg(key, vector<FieldValueTuple>(), op);
}

void ZmqProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple>& values)
{
    for (const auto &value : values)
    {
        sendMsg(kfvKey(value), kfvFieldsValues(value), SET_COMMAND);
    }
}

void ZmqProducerStateTable::del(const std::vector<std::string>& keys)
{
    for (const auto &key : keys)
    {
        sendMsg(key, vector<FieldValueTuple>(), DEL_COMMAND);
    }
}

void ZmqProducerStateTable::sendMsg(
        const std::string& key,
        const std::vector<swss::FieldValueTuple>& values,
        const std::string& command)
{
    int serializedlen = (int)BinarySerializer::serializeBuffer(
                                                        m_sendbuffer.data(),
                                                        m_sendbuffer.size(),
                                                        key,
                                                        values,
                                                        command);

    SWSS_LOG_DEBUG("sending: %d", serializedlen);
    int zmq_err = 0;
    for (int i = 0; i <=  MQ_MAX_RETRY; ++i)
    {    
        if (!m_connected)
        {
            connect(m_endpoint);
        }

        int rc = zmq_send(m_socket, m_sendbuffer.data(), serializedlen, ZMQ_DONTWAIT);
        if (rc >= 0)
        {
            SWSS_LOG_DEBUG("zmq sended %d bytes", serializedlen);
            return;
        }

        zmq_err = zmq_errno();
        // sleep (z ^ retry time) * 10 ms
        int retry_delay = (int)pow(2.0, i) * 10;
        if (zmq_err == EINTR
            || zmq_err== EFSM)
        {
            // EINTR: interrupted by signal
            // EFSM: socket state not ready
            //       For example when ZMQ socket still not receive reply message from last sended package.
            //       There was state machine inside ZMQ socket, when the socket is not in ready to send state, this error will happen.
            // for more detail, please check: http://api.zeromq.org/2-1:zmq-send
            SWSS_LOG_DEBUG("zmq send retry, endpoint: %s, error: %d", m_endpoint.c_str(), zmq_err);

            retry_delay = 0;
        }
        else if (zmq_err == EAGAIN)
        {
            // EAGAIN: ZMQ is full to need try again
            SWSS_LOG_WARN("zmq is full, will retry in %d ms, endpoint: %s, error: %d", retry_delay, m_endpoint.c_str(), zmq_err);
        }
        else if (zmq_err == ETERM)
        {
            // reconnect and send again.
            m_connected = false;
            SWSS_LOG_DEBUG("zmq connection break, endpoint: %s,error: %d", m_endpoint.c_str(), rc);
        }
        else
        {
            // for other error, send failed immediately.
            SWSS_LOG_THROW("zmq send failed, endpoint: %s, error: %d", m_endpoint.c_str(), rc);
        }

        usleep(retry_delay * 1000);
    }

    // failed after retry
    SWSS_LOG_ERROR("zmq_send on endpoint %s failed, zmqerrno: %d: %s, msg length: %d",
            m_endpoint.c_str(),
            zmq_err,
            zmq_strerror(zmq_err),
            serializedlen);
}

}
