#include "sonicv2connector.h"
#include "dbconnector.h"
#include "logger.h"

using namespace swss;

SonicV2Connector::SonicV2Connector(bool use_unix_socket_path, const char *netns)
    : m_use_unix_socket_path(use_unix_socket_path)
    , m_netns(netns)
{
}

std::string SonicV2Connector::getNamespace() const
{
    return m_netns;
}

void SonicV2Connector::connect(const std::string& db_name, bool retry_on)
{
    if (m_use_unix_socket_path)
    {
        m_dbintf.set_redis_kwargs(get_db_socket(db_name), "", 0);
    }
    else
    {
        m_dbintf.set_redis_kwargs("", get_db_hostname(db_name), get_db_port(db_name));
    }
    int db_id = get_dbid(db_name);
    m_dbintf.connect(db_id, db_name, retry_on);
}

void SonicV2Connector::close(const std::string& db_name)
{
    m_dbintf.close(db_name);
}

std::vector<std::string> SonicV2Connector::get_db_list()
{
    return SonicDBConfig::getDbList(m_netns);
}

int SonicV2Connector::get_dbid(const std::string& db_name)
{
    return SonicDBConfig::getDbId(db_name, m_netns);
}

std::string SonicV2Connector::get_db_separator(const std::string& db_name)
{
    return SonicDBConfig::getSeparator(db_name, m_netns);
}

DBConnector& SonicV2Connector::get_redis_client(const std::string& db_name)
{
    return m_dbintf.get_redis_client(db_name);
}

int64_t SonicV2Connector::publish(const std::string& db_name, const std::string& channel, const std::string& message)
{
    return m_dbintf.publish(db_name, channel, message);
}

bool SonicV2Connector::exists(const std::string& db_name, const std::string& key)
{
    return m_dbintf.exists(db_name, key);
}

std::vector<std::string> SonicV2Connector::keys(const std::string& db_name, const char *pattern)
{
    return m_dbintf.keys(db_name, pattern);
}

std::string SonicV2Connector::get(const std::string& db_name, const std::string& _hash, const std::string& key)
{
    return m_dbintf.get(db_name, _hash, key);
}

std::map<std::string, std::string> SonicV2Connector::get_all(const std::string& db_name, const std::string& _hash)
{
    return m_dbintf.get_all(db_name, _hash);
}

int64_t SonicV2Connector::set(const std::string& db_name, const std::string& _hash, const std::string& key, const std::string& val)
{
    return m_dbintf.set(db_name, _hash, key, val);
}

int64_t SonicV2Connector::del(const std::string& db_name, const std::string& key)
{
    return m_dbintf.del(db_name, key);
}

void SonicV2Connector::delete_all_by_pattern(const std::string& db_name, const std::string& pattern)
{
    m_dbintf.delete_all_by_pattern(db_name, pattern);
}

std::string SonicV2Connector::get_db_socket(const std::string& db_name)
{
    return SonicDBConfig::getDbSock(db_name, m_netns);
}

std::string SonicV2Connector::get_db_hostname(const std::string& db_name)
{
    return SonicDBConfig::getDbHostname(db_name, m_netns);
}

int SonicV2Connector::get_db_port(const std::string& db_name)
{
    return SonicDBConfig::getDbPort(db_name, m_netns);
}
