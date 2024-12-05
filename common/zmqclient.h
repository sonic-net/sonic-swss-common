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
    ZmqClient(const std::string& endpoint, const std::string& vrf);
    ~ZmqClient();

    bool isConnected();

    void connect();

    void sendMsg(const std::string& dbName,
                 const std::string& tableName,
                 const std::vector<KeyOpFieldsValuesTuple>& kcos);
private:
    void initialize(const std::string& endpoint, const std::string& vrf);

    std::string m_endpoint;

    std::string m_vrf;

    void* m_context;

    void* m_socket;

    bool m_connected;

    std::mutex m_socketMutex;
    
    std::vector<char> m_sendbuffer;
};

}
