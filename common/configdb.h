#pragma once

#include <string>
#include <map>
#include "sonicv2connector.h"

namespace swss {

class ConfigDBConnector : public SonicV2Connector
{
public:
    ConfigDBConnector(bool use_unix_socket_path = false, const char *netns = "");

    void db_connect(std::string db_name, bool wait_for_init, bool retry_on);
    void connect(bool wait_for_init = true, bool retry_on = false);

    void set_entry(std::string table, std::string key, const std::unordered_map<std::string, std::string>& data);
    void mod_entry(std::string table, std::string key, const std::unordered_map<std::string, std::string>& data);
    std::unordered_map<std::string, std::string> get_entry(std::string table, std::string key);
    std::vector<std::string> get_keys(std::string table, bool split = true);
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> get_table(std::string table);
    void delete_table(std::string table);
    void mod_config(const std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>& data);
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> get_config();

protected:
    static constexpr const char *INIT_INDICATOR = "CONFIG_DB_INITIALIZED";
    static constexpr const char *TABLE_NAME_SEPARATOR = "|";
    static constexpr const char *KEY_SEPARATOR = "|";

    std::string m_db_name;
};

}
