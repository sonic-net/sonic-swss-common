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

    // Cap the inner EAGAIN back-off ladder. By default sendMsg() runs the full
    // MQ_MAX_RETRY ladder (≈41s worst case on a single message). A producer that
    // owns an outer retry loop (e.g. a dedicated send thread that re-queues the
    // failed batch and lets new writes coalesce onto the same keys) sets a small
    // cap so control returns quickly instead of blocking on one message.
    //   maxRetries  < 0 keeps the default (MQ_MAX_RETRY).
    //   maxBackoffMs < 0 keeps the default (uncapped exponential doubling);
    //                >= 0 clamps each retry sleep to that ceiling.
    // Pair a large maxRetries with a maxBackoffMs ceiling to bound the per-retry
    // sleep. This is set-once configuration: call it during setup before any
    // send begins (it is not synchronized against a concurrent sendMsg()).
    void setSendRetryConfig(int maxRetries, int maxBackoffMs = -1);

    // Send-path back-pressure counters. sendMsg() drives the ZMQ socket in
    // non-blocking mode and retries with exponential backoff when the socket is
    // full (EAGAIN) or transiently unready (EINTR/EFSM). Today that signal is
    // only logged; these process-local counters expose it so a producer can
    // publish it as a leading indicator of downstream congestion. The class
    // stays DB-agnostic — it only counts; the owner reads via the getters and
    // decides where/how to publish. Counters are per-ZmqClient instance.
    uint64_t getSendEagainTotal() const { return m_sendEagainTotal.load(std::memory_order_relaxed); }
    uint64_t getSendRetryTotal() const { return m_sendRetryTotal.load(std::memory_order_relaxed); }
    uint64_t getSendBackoffMaxMs() const { return m_sendBackoffMaxMs.load(std::memory_order_relaxed); }
    uint64_t getSendLadderExhaustedTotal() const { return m_sendLadderExhaustedTotal.load(std::memory_order_relaxed); }

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

    // Inner retry-ladder caps (see setSendRetryConfig). Defaults preserve the
    // historical behavior for every existing producer.
    int m_sendMaxRetries = MQ_MAX_RETRY;
    int m_sendMaxBackoffMs = -1;

    // Send-path back-pressure counters (see accessor doc above). Trailing
    // members: ZmqClient is only ever held by reference/pointer (never embedded
    // by value), so appending members keeps the change additive for consumers.
    std::atomic<uint64_t> m_sendEagainTotal{0};
    std::atomic<uint64_t> m_sendRetryTotal{0};
    std::atomic<uint64_t> m_sendBackoffMaxMs{0};
    std::atomic<uint64_t> m_sendLadderExhaustedTotal{0};
};

}
