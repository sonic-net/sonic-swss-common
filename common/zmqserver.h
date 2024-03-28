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
static const int ORCH_ZMQ_PORT = 8020;

namespace swss {

class ZmqMessageHandler
{
public:
    virtual ~ZmqMessageHandler() {};
    virtual void handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>>& kcos) = 0;
};

class ZmqServer
{
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    ZmqServer(const std::string& endpoint);
    ~ZmqServer();

    std::string endpoint();

    void connect();

    void close();

    void registerMessageHandler(
                                const std::string dbName,
                                const std::string tableName,
                                ZmqMessageHandler* handler);

private:
    void handleReceivedData(const char* buffer, const size_t size);

    void mqPollThread();
    
    ZmqMessageHandler* findMessageHandler(const std::string dbName, const std::string tableName);

    std::vector<char> m_buffer;

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_mqPollThread;

    std::string m_endpoint;

    std::map<std::string, std::map<std::string, ZmqMessageHandler*>> m_HandlerMap;
};

}
