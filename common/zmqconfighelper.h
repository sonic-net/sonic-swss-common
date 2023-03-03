#pragma once

#include <string>
#include "json.hpp"

#define ZMQ_PORT_MAPPING_FILE           "/etc/sonic/zmp_port_mapping.json" 
#define ZMQ_PORT_CONFIG_NAME            "port" 

namespace swss {

class ZmqConfigHelper
{
public:
    static int GetSocketPort(std::string db, std::string tableName, std::string configPath = ZMQ_PORT_MAPPING_FILE);
};

}
