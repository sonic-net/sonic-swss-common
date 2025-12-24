#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <utility>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <system_error>
#include <zmq.h>
#include "zmqclient.h"
#include "binaryserializer.h"

using namespace std;

namespace swss {

ZmqClient::ZmqClient(const std::string& endpoint)
    : ZmqClient(endpoint, "")
{
}

ZmqClient::ZmqClient(const std::string& endpoint, const std::string& vrf)
    : m_waitTimeMs(0), m_oneToOneSync(false)
{
    initialize(endpoint, vrf);
}

ZmqClient::ZmqClient(const std::string& endpoint, uint32_t waitTimeMs)
    : m_waitTimeMs(waitTimeMs), m_oneToOneSync(false)
{
    initialize(endpoint);
}

ZmqClient::~ZmqClient()
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
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
    
void ZmqClient::initialize(const std::string& endpoint, const std::string& vrf)
{
    m_connected = false;
    m_endpoint = endpoint;
    m_context = nullptr;
    m_socket = nullptr;
    m_vrf = vrf;
    m_sendbuffer.resize(MQ_RESPONSE_MAX_COUNT);

    connect();
}

bool ZmqClient::isConnected()
{
    return m_connected;
}
    
void ZmqClient::connect()
{
    if (m_connected)
    {
        SWSS_LOG_DEBUG("Already connected to endpoint: %s", m_endpoint.c_str());
        return;
    }

    std::lock_guard<std::mutex> lock(m_socketMutex);
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
    
    m_context = zmq_ctx_new();
    if (m_oneToOneSync)
    {
        m_socket = zmq_socket(m_context, ZMQ_REQ);
    }
    else
    {
        m_socket = zmq_socket(m_context, ZMQ_PUSH);
    }

    // timeout all pending send package, so zmq will not block in dtor of this class: http://api.zeromq.org/master:zmq-setsockopt
    int linger = 0;
    zmq_setsockopt(m_socket, ZMQ_LINGER, &linger, sizeof(linger));

    if (!m_oneToOneSync)
    {
        // Increase send buffer for use all bandwidth: http://api.zeromq.org/4-2:zmq-setsockopt
        int high_watermark = MQ_WATERMARK;
        zmq_setsockopt(m_socket, ZMQ_SNDHWM, &high_watermark, sizeof(high_watermark));
    }

    if (!m_vrf.empty())
    {
        zmq_setsockopt(m_socket, ZMQ_BINDTODEVICE, m_vrf.c_str(), m_vrf.length());
    }

    SWSS_LOG_NOTICE("connect to zmq endpoint: %s", m_endpoint.c_str());
    int rc = zmq_connect(m_socket, m_endpoint.c_str());

    if (rc != 0)
    {
        m_connected = false;
        SWSS_LOG_THROW("failed to connect to zmq endpoint %s, zmqerrno: %d",
                m_endpoint.c_str(),
                zmq_errno());
    }

    m_connected = true;
}

void ZmqClient::sendMsg(
        const std::string& dbName,
        const std::string& tableName,
        const std::vector<KeyOpFieldsValuesTuple>& kcos)
{
    int serializedlen = (int)BinarySerializer::serializeBuffer(
                                                        m_sendbuffer.data(),
                                                        m_sendbuffer.size(),
                                                        dbName,
                                                        tableName,
                                                        kcos);

    if (serializedlen >= MQ_RESPONSE_MAX_COUNT)
    {
        SWSS_LOG_THROW("ZmqClient sendMsg message was too big (buffer size %d bytes, got %d), reduce the message size, message DROPPED",
                MQ_RESPONSE_MAX_COUNT,
                serializedlen);
    }

    SWSS_LOG_DEBUG("sending: %d", serializedlen);
    int zmq_err = 0;
    int retry_delay = 10;
    int rc = 0;
    for (int i = 0; i <= MQ_MAX_RETRY; ++i)
    {
        {
            // ZMQ socket is not thread safe: http://api.zeromq.org/2-1:zmq
            std::lock_guard<std::mutex> lock(m_socketMutex);

            // Use none block mode to use all bandwidth: http://api.zeromq.org/2-1%3Azmq-send
            if (m_oneToOneSync)
            {
                rc = zmq_send(m_socket, m_sendbuffer.data(), serializedlen, 0);
            }
            else
            {
                rc = zmq_send(m_socket, m_sendbuffer.data(), serializedlen, ZMQ_NOBLOCK);
            }
        }
        if (rc >= 0)
        {
            SWSS_LOG_DEBUG("zmq sended %d bytes", serializedlen);
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
            m_connected = false;
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

bool ZmqClient::wait(
    std::string& dbName, std::string& tableName,
    std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) {

    SWSS_LOG_ENTER();

    if (!m_oneToOneSync)
    {
        return false;
    }

    zmq_pollitem_t items [1] = { };
    items[0].socket = m_socket;
    items[0].events = ZMQ_POLLIN;

    int rc;

    for (int i = 0; true; ++i)
    {
        rc = zmq_poll(items, 1, (int)m_waitTimeMs);

        if (rc == 0)
        {
            SWSS_LOG_ERROR("zmq_poll timed out");
            return false;
        }
        if (rc > 0)
        {
            break;
        }
        if (zmq_errno() == EINTR && i <= MQ_MAX_RETRY)
        {
            continue;
        }
        SWSS_LOG_THROW("zmq_poll failed, zmqerrno: %d", zmq_errno());
    }

    for (int i = 0; true; ++i)
    {
        rc = zmq_recv(m_socket, m_sendbuffer.data(), m_sendbuffer.size(), 0);

        if (rc < 0)
        {
            if (zmq_errno() == EINTR && i <= MQ_MAX_RETRY)
            {
                continue;
            }
            SWSS_LOG_THROW("zmq_recv failed, zmqerrno: %d", zmq_errno());
        }
        if (rc >= (int)m_sendbuffer.size())
        {
            SWSS_LOG_THROW(
                "zmq_recv message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                (int)m_sendbuffer.size(), rc);
        }
        break;
    }

    m_sendbuffer.at(rc) = 0; // make sure that we end string with zero before parse
    kcos.clear();
    BinarySerializer::deserializeBuffer(m_sendbuffer.data(), m_sendbuffer.size(), dbName, tableName, kcos);

  return true;
}
}
