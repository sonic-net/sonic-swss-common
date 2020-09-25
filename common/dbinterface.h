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

class DBInterface : public RedisContext
{
public:
    void connect(int dbId, bool retry = true);
    void close(int dbId);
    int64_t del(int dbId, const std::string& key);
    bool exists(int dbId, const std::string& key);
    std::string get(int dbId, const std::string& hash, const std::string& key);
    std::map<std::string, std::string> get_all(int dbId, const std::string& hash, bool blocking = false);
    std::vector<std::string> keys(int dbId, const std::string& pattern = "*", bool blocking = false);
    int64_t publish(int dbId, const std::string& channel, const std::string& message);
    int64_t set(int dbId, const std::string& hash, const std::string& key, const std::string& value, bool blocking = false);
    DBConnector& get_redis_client(int dbId);

private:
    template <typename FUNC>
    auto blockable(FUNC f, int dbId, bool blocking = false) -> decltype(f());
    // Unsubscribe the chosent client from keyspace event notifications
    void _unsubscribe_keyspace_notification(int dbId);
    bool _unavailable_data_handler(int dbId, const char *data);
    // Subscribe the chosent client to keyspace event notifications
    void _subscribe_keyspace_notification(int dbId);
    // In the event Redis is unavailable, close existing connections, and try again.
    void _connection_error_handler(int dbId);
    void _onetime_connect(int dbId);
    // Keep reconnecting to Database 'dbId' until success
    void _persistent_connect(int dbId);

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

    std::unordered_map<int, std::shared_ptr<DBConnector>> keyspace_notification_channels;

    std::unordered_map<int, DBConnector> m_redisClient;
};

}
