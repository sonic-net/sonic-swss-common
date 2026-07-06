#pragma once

#include <map>
#include <memory>
#include <mutex>
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
};

// Shared (db, table) -> handler map. Co-owned by ZmqServer and every
// registered handler so neither party requires the other to outlive it.
// The mutex is held across handler dispatch, so removeHandler() blocks
// until any in-flight callback into the handler being removed has returned
// — making it safe for the caller to destroy the handler immediately after
// removeHandler() returns. Handler callbacks therefore must not themselves
// call registerHandler / removeHandler (would self-deadlock).
class ZmqHandlerRegistry
{
public:
    void registerHandler(const std::string& dbName,
                         const std::string& tableName,
                         ZmqMessageHandler* handler);

    void removeHandler(const std::string& dbName,
                       const std::string& tableName);

    void dispatch(const std::string& dbName,
                  const std::string& tableName,
                  const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>>& kcos);

private:
    std::mutex m_mutex;
    std::map<std::string, std::map<std::string, ZmqMessageHandler*>> m_handlers;
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
    ~ZmqServer();

    void registerMessageHandler(
                                const std::string dbName,
                                const std::string tableName,
                                ZmqMessageHandler* handler);

    // Remove a previously-registered handler. Blocks until any in-flight
    // dispatch into that handler has returned, so the caller can safely
    // destroy the handler object after this call returns.
    void removeMessageHandler(
                                const std::string& dbName,
                                const std::string& tableName);

    // This method should only be used in one-to-one sync mode with the client.
    void sendMsg(const std::string& dbName, const std::string& tableName,
        const std::vector<swss::KeyOpFieldsValuesTuple>& values);

    void bind();

    // Internal: returns the shared handler registry so a handler implementation
    // can co-own it. This lets the handler's destructor call removeHandler()
    // without depending on the ZmqServer still being alive (the registry
    // survives as long as either party holds a reference). Intended for use
    // by ZmqMessageHandler subclasses, not by general callers.
    std::shared_ptr<ZmqHandlerRegistry> getHandlerRegistry() const { return m_registry; }

private:
    void handleReceivedData(const char* buffer, const size_t size);

    void startMqPollThread();

    void mqPollThread();

    // Retained as a no-op stub for source compatibility with external test
    // mocks (e.g. sonic-swss's fake_zmqserver.cpp) that override this symbol
    // at link time. The internal dispatch path no longer calls it — handler
    // lookup happens inside ZmqHandlerRegistry::dispatch().
    ZmqMessageHandler* findMessageHandler(const std::string dbName, const std::string tableName);

    std::vector<char> m_buffer;

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_mqPollThread;

    std::string m_endpoint;

    std::string m_vrf;

    void* m_context;

    void* m_socket;

    bool m_oneToOneSync = false;

    bool m_allowZmqPoll;

    // Default-initialized in-class so that link-time mocks of ZmqServer
    // (which may not initialize this member in their stub constructors) still
    // present a valid registry to any real ZmqConsumerStateTable they pair
    // with — the real constructor below also sets this explicitly.
    std::shared_ptr<ZmqHandlerRegistry> m_registry = std::make_shared<ZmqHandlerRegistry>();
};

}
