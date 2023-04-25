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
    : m_endpoint(endpoint)
{
    m_mqPollThread = std::make_shared<std::thread>(&ZmqServer::mqPollThread, this);
    m_runThread = true;

    SWSS_LOG_DEBUG("ZmqServer ctor endpoint: %s", endpoint.c_str());
}

ZmqServer::~ZmqServer()
{
    m_runThread = false;
    m_mqPollThread->join();
}

void ZmqServer::registerMessageHandler(
                                    const std::string dbName,
                                    const std::string tableName,
                                    ZmqMessageHandler* handler)
{
    auto result = m_HandlerMap.insert(dbName);
    if (result.second) {
        std::map<std::string, ZmqMessageHandler*> tableMapping;
        result.first = tableMapping;
    }

    auto result = result.first.find(tableName);
    if (result.second) {
        result.first = handler;

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
    auto pkco = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *pkco;
    auto& values = kfvFieldsValues(kco);
    BinarySerializer::deserializeBuffer(buffer, size, values);

    // get table name
    swss::FieldValueTuple fvt = values.at(0);
    string dbName = fvField(fvt);
    string tableName = fvValue(fvt);
    values.erase(values.begin());

    // find handler
    auto handler = findMessageHandler(dbName, tableName);
    if (handler == nullptr) {
        SWSS_LOG_WARN("ZmqServer can't find handler for received message: %s", buffer);
        return;
    }

    // get key and OP
    fvt = values.at(0);
    kfvKey(kco) = fvField(fvt);
    kfvOp(kco) = fvValue(fvt);
    values.erase(values.begin());

    handler->handleReceivedData(pkco);
}

void ZmqServer::mqPollThread()
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
        rc = zmq_poll(poll_item, 1, 1000);
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

}
