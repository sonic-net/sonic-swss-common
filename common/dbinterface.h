#pragma once

#include <stdint.h>
#include <unistd.h>
#include <stdexcept>

#include "dbconnector.h"
#include "redisclient.h"
#include "logger.h"

namespace swss
{

class UnavailableDataError : public std::runtime_error
{
public:
    UnavailableDataError(const std::string& message, const std::string& data)
        : std::runtime_error(message)
        , m_data(data)
    {
    }

    const char *getData() const
    {
        return m_data.c_str();
    }

private:
    const std::string m_data;
};

class DBInterface
{
public:
    void connect(int dbId, const std::string& dbName, bool retry = true);
    void close(const std::string& dbName);
    int64_t del(const std::string& dbName, const std::string& key, bool blocking = false);
    // Delete all keys which match %pattern from DB
    void delete_all_by_pattern(const std::string& dbName, const std::string& pattern);
    bool exists(const std::string& dbName, const std::string& key);
    std::string get(const std::string& dbName, const std::string& hash, const std::string& key, bool blocking = false);
    std::map<std::string, std::string> get_all(const std::string& dbName, const std::string& hash, bool blocking = false);
    std::vector<std::string> keys(const std::string& dbName, const char *pattern = "*", bool blocking = false);
    int64_t publish(const std::string& dbName, const std::string& channel, const std::string& message);
    int64_t set(const std::string& dbName, const std::string& hash, const std::string& key, const std::string& value, bool blocking = false);
    DBConnector& get_redis_client(const std::string& dbName);
    void set_redis_kwargs(std::string unix_socket_path, std::string host, int port);

private:
    template <typename T, typename FUNC>
    T blockable(FUNC f, const std::string& dbName, bool blocking = false);
    // Unsubscribe the chosent client from keyspace event notifications
    void _unsubscribe_keyspace_notification(const std::string& dbName);
    bool _unavailable_data_handler(const std::string& dbName, const char *data);
    // Subscribe the chosent client to keyspace event notifications
    void _subscribe_keyspace_notification(const std::string& dbName);
    // In the event Redis is unavailable, close existing connections, and try again.
    void _connection_error_handler(const std::string& dbName);
    void _onetime_connect(int dbId, const std::string& dbName);
    // Keep reconnecting to Database 'dbId' until success
    void _persistent_connect(int dbId, const std::string& dbName);

    static const int BLOCKING_ATTEMPT_ERROR_THRESHOLD = 10;
    static const int BLOCKING_ATTEMPT_SUPPRESSION = BLOCKING_ATTEMPT_ERROR_THRESHOLD + 5;

    // Wait period in seconds before attempting to reconnect to Redis.
    static const int CONNECT_RETRY_WAIT_TIME = 10;

    // Wait period in seconds to wait before attempting to retrieve missing data.
    static const int DATA_RETRIEVAL_WAIT_TIME = 3;

    // Time to wait for any given message to arrive via pub-sub.
    static constexpr double PUB_SUB_NOTIFICATION_TIMEOUT = 10.0;  // seconds

    // Maximum allowable time to wait on a specific pub-sub notification.
    static constexpr double PUB_SUB_MAXIMUM_DATA_WAIT = 60.0;  // seconds

    // Pub-sub keyspace pattern
    static constexpr const char *KEYSPACE_PATTERN = "__key*__:*";

    // In Redis, by default keyspace events notifications are disabled because while not
    // very sensible the feature uses some CPU power. Notifications are enabled using
    // the notify-keyspace-events of redis.conf or via the CONFIG SET.
    // In order to enable the feature a non-empty string is used, composed of multiple characters,
    // where every character has a special meaning according to the following table:
    // K - Keyspace events, published with __keyspace@<db>__ prefix.
    // E - Keyevent events, published with __keyevent@<db>__ prefix.
    // g - Generic commands (non-type specific) like DEL, EXPIRE, RENAME, ...
    // $ - String commands
    // l - List commands
    // s - Set commands
    // h - Hash commands
    // z - Sorted set commands
    // x - Expired events (events generated every time a key expires)
    // e - Evicted events (events generated when a key is evicted for maxmemory)
    // A - Alias for g$lshzxe, so that the "AKE" string means all the events.
    // ACS Redis db mainly uses hash, therefore h is selected.
    static constexpr const char *KEYSPACE_EVENTS = "KEA";

    std::unordered_map<std::string, std::shared_ptr<DBConnector>> keyspace_notification_channels;

    std::unordered_map<std::string, DBConnector> m_redisClient;

    std::string m_unix_socket_path;
    std::string m_host = "127.0.0.1";
    int m_port = 6379;
};

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
    SonicV2Connector(bool use_unix_socket_path = false, const char *netns = "")
        : m_use_unix_socket_path(use_unix_socket_path)
        , m_netns(netns)
    {
    }
#endif

    void connect(const std::string& db_name, bool retry_on = true)
    {
        if (m_use_unix_socket_path)
        {
            SWSS_LOG_INFO("connec1: %s", get_db_socket(db_name).c_str());
            dbintf.set_redis_kwargs(get_db_socket(db_name), "", 0);
        }
        else
        {
            SWSS_LOG_INFO("connec2: %s %d", get_db_hostname(db_name).c_str(), get_db_port(db_name));
            dbintf.set_redis_kwargs("", get_db_hostname(db_name), get_db_port(db_name));
        }
        int db_id = get_dbid(db_name);
        dbintf.connect(db_id, db_name, retry_on);
    }

    void close(const std::string& db_name)
    {
        dbintf.close(db_name);
    }

    std::vector<std::string> get_db_list()
    {
        return SonicDBConfig::getDbList(m_netns);
    }

    int get_dbid(const std::string& db_name)
    {
        return SonicDBConfig::getDbId(db_name, m_netns);
    }

    std::string get_db_separator(const std::string& db_name)
    {
        return SonicDBConfig::getSeparator(db_name, m_netns);
    }

    DBConnector& get_redis_client(const std::string& db_name)
    {
        return dbintf.get_redis_client(db_name);
    }

    int64_t publish(const std::string& db_name, const std::string& channel, const std::string& message)
    {
        return dbintf.publish(db_name, channel, message);
    }

    bool exists(const std::string& db_name, const std::string& key)
    {
        return dbintf.exists(db_name, key);
    }

    std::vector<std::string> keys(const std::string& db_name, const char *pattern="*")
    {
        return dbintf.keys(db_name, pattern);
    }

    std::string get(const std::string& db_name, const std::string& _hash, const std::string& key)
    {
        return dbintf.get(db_name, _hash, key);
    }

    std::map<std::string, std::string> get_all(const std::string& db_name, const std::string& _hash)
    {
        return dbintf.get_all(db_name, _hash);
    }

    int64_t set(const std::string& db_name, const std::string& _hash, const std::string& key, const std::string& val)
    {
        return dbintf.set(db_name, _hash, key, val);
    }

    int64_t del(const std::string& db_name, const std::string& key)
    {
        return dbintf.del(db_name, key);
    }

    void delete_all_by_pattern(const std::string& db_name, const std::string& pattern)
    {
        dbintf.delete_all_by_pattern(db_name, pattern);
    }

private:
    std::string get_db_socket(const std::string& db_name)
    {
        return SonicDBConfig::getDbSock(db_name, m_netns);
    }

    std::string get_db_hostname(const std::string& db_name)
    {
        return SonicDBConfig::getDbHostname(db_name, m_netns);
    }

    int get_db_port(const std::string& db_name)
    {
        return SonicDBConfig::getDbPort(db_name, m_netns);
    }

    DBInterface dbintf;
    bool m_use_unix_socket_path;
    std::string m_netns;
};

}
