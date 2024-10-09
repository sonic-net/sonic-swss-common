#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include <zmq.h>
#include <pthread.h>
#include "zmqserver.h"
#include "binaryserializer.h"

using namespace std;

namespace swss {

ZmqServer::ZmqServer(const std::string& endpoint)
    : ZmqServer(endpoint, "")
{
}

ZmqServer::ZmqServer(const std::string& endpoint, const std::string& vrf)
    : m_endpoint(endpoint),
    m_vrf(vrf)
{
    m_buffer.resize(MQ_RESPONSE_MAX_COUNT);
    m_runThread = true;
    m_mqPollThread = std::make_shared<std::thread>(&ZmqServer::mqPollThread, this);

    SWSS_LOG_DEBUG("ZmqServer ctor endpoint: %s", endpoint.c_str());
}

ZmqServer::~ZmqServer()
{
    m_allowZmqPoll = true;
    m_runThread = false;
    m_mqPollThread->join();

    zmq_close(m_socket);
}

void ZmqServer::registerMessageHandler(
                                    const std::string dbName,
                                    const std::string tableName,
                                    ZmqMessageHandler* handler)
{
    auto dbResult = m_HandlerMap.insert(pair<string, map<string, ZmqMessageHandler*>>(dbName, map<string, ZmqMessageHandler*>()));
    if (dbResult.second) {
        SWSS_LOG_DEBUG("ZmqServer add handler mapping for db: %s", dbName.c_str());
    }

    auto tableResult = dbResult.first->second.insert(pair<string, ZmqMessageHandler*>(tableName, handler));
    if (tableResult.second) {
        SWSS_LOG_DEBUG("ZmqServer register handler for db: %s, table: %s", dbName.c_str(), tableName.c_str());
    }
}

ZmqMessageHandler* ZmqServer::findMessageHandler(
                                                const std::string dbName,
                                                const std::string tableName)
{
    auto dbMappingIter = m_HandlerMap.find(dbName);
    if (dbMappingIter == m_HandlerMap.end()) {
        SWSS_LOG_DEBUG("ZmqServer can't find any handler for db: %s", dbName.c_str());
        return nullptr;
    }

    auto tableMappingIter = dbMappingIter->second.find(tableName);
    if (tableMappingIter == dbMappingIter->second.end()) {
        SWSS_LOG_DEBUG("ZmqServer can't find handler for db: %s, table: %s", dbName.c_str(), tableName.c_str());
        return nullptr;
    }

    return tableMappingIter->second;
}

void ZmqServer::handleReceivedData(const char* buffer, const size_t size)
{
    std::string dbName;
    std::string tableName;
    std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> kcos;
    BinarySerializer::deserializeBuffer(buffer, size, dbName, tableName, kcos);

    // find handler
    auto handler = findMessageHandler(dbName, tableName);
    if (handler == nullptr) {
        SWSS_LOG_WARN("ZmqServer can't find handler for received message: %s", buffer);
        return;
    }

    handler->handleReceivedData(kcos);
}

void ZmqServer::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");

    // Producer/Consumer state table are n:1 mapping, so need use PUSH/PULL pattern http://api.zeromq.org/master:zmq-socket
    void* context = zmq_ctx_new();;
    void* socket = zmq_socket(context, ZMQ_PULL);

    // Increase recv buffer for use all bandwidth:  http://api.zeromq.org/4-2:zmq-setsockopt
    int high_watermark = MQ_WATERMARK;
    zmq_setsockopt(socket, ZMQ_RCVHWM, &high_watermark, sizeof(high_watermark));

    if (!m_vrf.empty())
    {
        zmq_setsockopt(socket, ZMQ_BINDTODEVICE, m_vrf.c_str(), m_vrf.length());
    }

    int rc = zmq_bind(socket, m_endpoint.c_str());
    if (rc != 0)
    {
        SWSS_LOG_THROW("zmq_bind failed on endpoint: %s, zmqerrno: %d",
                m_endpoint.c_str(),
                zmq_errno());
    }

    // zmq_poll will use less CPU
    zmq_pollitem_t poll_item;
    poll_item.fd = 0;
    poll_item.socket = socket;
    poll_item.events = ZMQ_POLLIN;
    poll_item.revents = 0;

    SWSS_LOG_NOTICE("bind to zmq endpoint: %s", m_endpoint.c_str());
    while (m_runThread)
    {
        // receive message
        rc = zmq_poll(&poll_item, 1, 1000);
        if (rc == 0 || !(poll_item.revents & ZMQ_POLLIN))
        {
            // timeout or other event
            SWSS_LOG_DEBUG("zmq_poll timeout or invalied event rc: %d, revents: %d", rc, poll_item.revents);
            continue;
        }

        // receive message
        rc = zmq_recv(socket, m_buffer.data(), MQ_RESPONSE_MAX_COUNT, ZMQ_DONTWAIT);
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

        m_buffer.at(rc) = 0; // make sure that we end string with zero before parse
        SWSS_LOG_DEBUG("zmq received %d bytes", rc);

        // deserialize and write to redis:
        handleReceivedData(m_buffer.data(), rc);
    }

    zmq_close(socket);
    zmq_ctx_destroy(context);

    SWSS_LOG_NOTICE("mqPollThread end");
}

void ZmqServer::sendMsg(const std::string& dbName, const std::string& tableName,
        const std::vector<swss::KeyOpFieldsValuesTuple>& values)
{
    int serializedlen = (int)BinarySerializer::serializeBuffer(
                                                        m_buffer.data(),
                                                        m_buffer.size(),
                                                        dbName,
                                                        tableName,
                                                        values);

    SWSS_LOG_DEBUG("sending: %d", serializedlen);
    int zmq_err = 0;
    int retry_delay = 10;
    int rc = 0;
    for (int i = 0; i <=  MQ_MAX_RETRY; ++i)
    {
        rc = zmq_send(m_socket, m_buffer.data(), serializedlen, 0);
        if (rc >= 0)
        {
            m_allowZmqPoll = true;
            SWSS_LOG_DEBUG("zmq sent %d bytes", serializedlen);
            return;
        }

        zmq_err = zmq_errno();
        // sleep (2 ^ retry time) * 10 ms
        retry_delay *= 2;
        if (zmq_err == EINTR
            || zmq_err== EFSM)
        {
            // EINTR: interrupted by signal
            // EFSM: socket state not ready
            //       For example when ZMQ socket still not receive reply message from last sended package.
            //       There was state machine inside ZMQ socket, when the socket is not in ready to send state, this
            //       error will happen.
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
            auto message =  "zmq connection break, endpoint: " + m_endpoint + ", error: " + to_string(rc);
            SWSS_LOG_ERROR("%s", message.c_str());
            throw system_error(make_error_code(errc::connection_reset), message);
        }
        else
        {
            // for other error, send failed immediately.
            auto message =  "zmq send failed, endpoint: " + m_endpoint + ", error: " + to_string(rc);
            SWSS_LOG_ERROR("%s", message.c_str());
            throw system_error(make_error_code(errc::io_error), message);
        }
        usleep(retry_delay * 1000);
    }
    // failed after retry
    auto message =  "zmq send failed, endpoint: " + m_endpoint + ", zmqerrno: " + to_string(zmq_err) + ":" + zmq_strerror(zmq_err) + ", msg length:" + to_string(serializedlen);
    SWSS_LOG_ERROR("%s", message.c_str());
    throw system_error(make_error_code(errc::io_error), message);
}

}
