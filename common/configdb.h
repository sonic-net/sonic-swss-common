#pragma once

#include <string>
#include "sonicv2connector.h"

namespace swss {

class ConfigDBConnector : public SonicV2Connector
{
public:
    ConfigDBConnector(bool use_unix_socket_path = false, const char *netns = "");

    void connect(bool wait_for_init = true, bool retry_on = false);

protected:
    static constexpr const char *INIT_INDICATOR = "CONFIG_DB_INITIALIZED";
    static constexpr const char *TABLE_NAME_SEPARATOR = "|";
    static constexpr const char *KEY_SEPARATOR = "|";

    std::string m_db_name;
};

}
