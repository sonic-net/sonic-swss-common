#pragma once

#include <string>
#include <deque>
#include <condition_variable>
#include <vector>
#include "table.h"

#define MQ_RESPONSE_MAX_COUNT (16*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10
#define MQ_POLL_TIMEOUT (1000)
#define MQ_WATERMARK 10000

/***** ZMQ PORT *****/
static const int ORCH_ZMQ_PORT = 8100;

namespace swss {

class ZmqMessageHandler
{
public:
    virtual ~ZmqMessageHandler() {};
    virtual void handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>>& kcos) = 0;
    /*
     * Called by ZmqRouteServer::mqPollThread once the socket is drained
     * (zmq_recv returned EAGAIN). Handlers use this to wake their consumer at
     * most once per burst rather than once per message. Default is no-op.
     */
    virtual void notifyPending() {}
};

class ZmqServer
{
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    // If oneToOneSync is set to non-zero, it will enable one-to-one sync with
    // the client. It will use ZMQ_REQ and ZMQ_REP socket type. There can only
    // be one client and one server for a ZMQ socket.
    ZmqServer(const std::string& endpoint);
    ZmqServer(const std::string& endpoint, const std::string& vrf);
    ZmqServer(const std::string& endpoint, const std::string& vrf, bool lazyBind);
    ZmqServer(const std::string& endpoint, const std::string& vrf, bool lazyBind, bool oneToOneSync);
    virtual ~ZmqServer();

    void registerMessageHandler(
                                const std::string dbName,
                                const std::string tableName,
                                ZmqMessageHandler* handler);

    // This method should only be used in one-to-one sync mode with the client.
    void sendMsg(const std::string& dbName, const std::string& tableName,
        const std::vector<swss::KeyOpFieldsValuesTuple>& values);

    void bind();

protected:
    ZmqMessageHandler * handleReceivedData(const char* buffer, const size_t size);

    virtual void mqPollThread();

    std::vector<char> m_buffer;

    volatile bool m_runThread;

private:
    void startMqPollThread();

    ZmqMessageHandler* findMessageHandler(const std::string dbName, const std::string tableName);

    std::shared_ptr<std::thread> m_mqPollThread;

protected:
    std::string m_endpoint;

private:
    std::string m_vrf;

    void* m_context;

protected:
    void* m_socket;

    bool m_oneToOneSync = false;

    bool m_allowZmqPoll;

private:
    std::map<std::string, std::map<std::string, ZmqMessageHandler*>> m_HandlerMap;
};

}
