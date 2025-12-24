#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <thread> 
#include <mutex> 
#include "zmqserver.h"

namespace swss {

class ZmqClient
{
public:

    ZmqClient(const std::string& endpoint);
    ZmqClient(const std::string& endpoint, const std::string& vrf = "");
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

private:
    void initialize(const std::string& endpoint, const std::string& vrf = "");

    std::string m_endpoint;

    std::string m_vrf;

    void* m_context;

    void* m_socket;

    bool m_connected;

    // Timeout in ms to wait for response message from the server.
    // If this is set to zero, one-to-one sync is disabled.
    uint32_t m_waitTimeMs;

    bool m_oneToOneSync;

    std::mutex m_socketMutex;

    std::vector<char> m_sendbuffer;
};

}
