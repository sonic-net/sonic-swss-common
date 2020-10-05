#pragma once

#include <stdint.h>
#include <unistd.h>

#include "dbinterface.h"

namespace swss
{

class SonicV2Connector
{
public:
#ifdef SWIG
    %pythoncode %{
        def __init__(self, use_unix_socket_path = False, namespace = None):
            self.m_use_unix_socket_path = use_unix_socket_path
            self.m_netns = namespace
    %}
#else
    SonicV2Connector(bool use_unix_socket_path = false, const char *netns = "");
#endif

    void connect(const std::string& db_name, bool retry_on = true);

    void close(const std::string& db_name);

    std::vector<std::string> get_db_list();

    int get_dbid(const std::string& db_name);

    std::string get_db_separator(const std::string& db_name);

    DBConnector& get_redis_client(const std::string& db_name);

    int64_t publish(const std::string& db_name, const std::string& channel, const std::string& message);

    bool exists(const std::string& db_name, const std::string& key);

    std::vector<std::string> keys(const std::string& db_name, const char *pattern="*");

    std::string get(const std::string& db_name, const std::string& _hash, const std::string& key);

    std::map<std::string, std::string> get_all(const std::string& db_name, const std::string& _hash);

    int64_t set(const std::string& db_name, const std::string& _hash, const std::string& key, const std::string& val);

    int64_t del(const std::string& db_name, const std::string& key);

    void delete_all_by_pattern(const std::string& db_name, const std::string& pattern);

private:
    std::string get_db_socket(const std::string& db_name);

    std::string get_db_hostname(const std::string& db_name);

    int get_db_port(const std::string& db_name);

    DBInterface m_dbintf;
    bool m_use_unix_socket_path;
    std::string m_netns;
};

}
