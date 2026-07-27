#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <thread> 
#include <mutex> 
#include <atomic>
#include <cstdint>
#include "zmqserver.h"

namespace swss {

class ZmqClient
{
public:

    ZmqClient(const std::string& endpoint);
    ZmqClient(const std::string& endpoint, const std::string& vrf);
    // If waitTimeMs is set to non-zero, it will enable one-to-one sync with the
    // server. It will use ZMQ_REQ and ZMQ_REP socket type. There can only be
    // one client and one server for a ZMQ socket.
    ZmqClient(const std::string& endpoint, uint32_t waitTimeMs);
    ~ZmqClient();

    bool isConnected();

    void connect();

    void sendMsg(const std::string& dbName,
                 const std::string& tableName,
                 const std::vector<KeyOpFieldsValuesTuple>& kcos);

    // This method should only be used in one-to-one sync mode with the server.
    bool wait(std::string& dbName,
              std::string& tableName,
              std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>>& kcos);

    // Optionally shorten the inner send-retry loop. Typically configured once
    // at setup, but the values are atomic so they may also be changed between
    // sends (each send snapshots them for a consistent read).
    // maxRetries   < 0 keeps the default ladder; otherwise clamped to MQ_MAX_RETRY.
    // maxBackoffMs < 0 keeps the exponential backoff; >= 0 caps each retry sleep (ms).
    void setSendRetryConfig(int maxRetries, int maxBackoffMs = -1);

    // Process-local send-path back-pressure counters (the socket-full signal
    // sendMsg() otherwise only logs). ZmqClient only counts; the owner publishes.
    uint64_t getSendEagainTotal() const { return m_sendEagainTotal.load(std::memory_order_relaxed); }        // total EAGAIN occurrences
    uint64_t getSendBlipAbsorbedTotal() const { return m_sendBlipAbsorbedTotal.load(std::memory_order_relaxed); } // EAGAIN sends that still succeeded
    uint64_t getSendBackoffMaxMs() const { return m_sendBackoffMaxMs.load(std::memory_order_relaxed); }       // deepest backoff waited (ms)

private:
    void initialize(const std::string& endpoint, const std::string& vrf = "");

    std::string m_endpoint;

    std::string m_vrf;

    void* m_context;

    void* m_socket;

    bool m_connected;

    // If this is set to zero, one-to-one sync is disabled.
    uint32_t m_waitTimeMs = 0;

    bool m_oneToOneSync = false;

    std::mutex m_socketMutex;

    std::vector<char> m_sendbuffer;

    // Inner send-retry caps (see setSendRetryConfig); defaults preserve behavior.
    std::atomic<int> m_sendMaxRetries{MQ_MAX_RETRY};
    std::atomic<int> m_sendMaxBackoffMs{-1};

    // Send-path back-pressure counters (see getters above). Appended members;
    // all swss-common consumers rebuild from source, and no existing call site
    // changes.
    std::atomic<uint64_t> m_sendEagainTotal{0};
    std::atomic<uint64_t> m_sendBlipAbsorbedTotal{0};
    std::atomic<uint64_t> m_sendBackoffMaxMs{0};
};

}
