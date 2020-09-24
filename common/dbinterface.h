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
    void connect(int dbId, bool retry = true)
    {
        if (retry)
        {
            _persistent_connect(dbId);
        }
        else
        {
            _onetime_connect(dbId);
        }
    }

    void close(int dbId)
    {
        m_redisClient.erase(dbId);
    }

    int64_t del(int dbId, const std::string& key)
    {
        return m_redisClient.at(dbId).del(key);
    }

    bool exists(int dbId, const std::string& key)
    {
        return m_redisClient.at(dbId).exists(key);
    }

    std::string get(int dbId, const std::string& hash, const std::string& key)
    {
        auto pvalue = m_redisClient.at(dbId).hget(hash, key);
        if (!pvalue)
        {
            std::string message = "Key '" + hash + "' field '" + key + "' unavailable in database '" + std::to_string(dbId) + "'";
            SWSS_LOG_WARN("%s", message.c_str());
            throw UnavailableDataError(message, hash);
        }
        const std::string& value = *pvalue;
        return value == "None" ? "" : value;
    }

    std::map<std::string, std::string> get_all(int dbId, const std::string& hash, bool blocking = false)
    {
        auto innerfunc = [&]
        {
            std::map<std::string, std::string> map;
            m_redisClient.at(dbId).hgetall(hash, std::inserter(map, map.end()));

            if (map.empty())
            {
                std::string message = "Key '{" + hash + "}' unavailable in database '{" + std::to_string(dbId) + "}'";
                SWSS_LOG_WARN("%s", message.c_str());
                throw UnavailableDataError(message, hash);
            }
            for (auto& i : map)
            {
                std::string& value = i.second;
                if (value == "None")
                {
                    value = "";
                }
            }

            return map;
        };
        return blockable(innerfunc, dbId, blocking);
    }

    std::vector<std::string> keys(int dbId, const std::string& pattern = "*", bool blocking = false)
    {
        auto innerfunc = [&]
        {
            auto keys = m_redisClient.at(dbId).keys(pattern);
            if (keys.empty())
            {
                std::string message = "DB '{" + std::to_string(dbId) + "}' is empty!";
                SWSS_LOG_WARN("%s", message.c_str());
                throw UnavailableDataError(message, "keys");
            }
            return keys;
        };
        return blockable(innerfunc, dbId, blocking);
    }

    int64_t publish(int dbId, const std::string& channel, const std::string& message)
    {
        throw std::logic_error("Not implemented");
    }

    int64_t set(int dbId, const std::string& hash, const std::string& key, const std::string& value, bool blocking = false)
    {
        auto innerfunc = [&]
        {
            m_redisClient.at(dbId).hset(hash, key, value);
            // Return the number of fields that were added.
            return 1;
        };
        return blockable(innerfunc, dbId, blocking);
    }

    DBConnector& get_redis_client(int dbId)
    {
        return m_redisClient.at(dbId);
    }

private:
    template <typename FUNC>
    auto blockable(FUNC f, int dbId, bool blocking = false) -> decltype(f())
    {
        typedef decltype(f()) T;
        int attempts = 0;
        for (;;)
        {
            try
            {
                T ret_data = f();
                _unsubscribe_keyspace_notification(dbId);
                return ret_data;
            }
            catch (const UnavailableDataError& e)
            {
                if (blocking)
                {
                    auto found = keyspace_notification_channels.find(dbId);
                    if (found != keyspace_notification_channels.end())
                    {
                        bool result = _unavailable_data_handler(dbId, e.getData());
                        if (result)
                        {
                            continue; // received updates, try to read data again
                        }
                        else
                        {
                            _unsubscribe_keyspace_notification(dbId);
                            throw; // No updates was received. Raise exception
                        }
                    }
                    else
                    {
                        // Subscribe to updates and try it again (avoiding race condition)
                        _subscribe_keyspace_notification(dbId);
                    }
                }
                else
                {
                    return T();
                }
            }
            catch (const std::system_error&)
            {
                /*
                Something is fundamentally wrong with the request itself.
                Retrying the request won't pass unless the schema itself changes. In this case, the error
                should be attributed to the application itself. Re-raise the error.
                */
                SWSS_LOG_ERROR("Bad DB request [%d]", dbId);
                throw;
            }
            catch (const RedisError&)
            {
                // Redis connection broken and we need to retry several times
                attempts += 1;
                _connection_error_handler(dbId);
                std::string msg = "DB access failure by [" + std::to_string(dbId) + + "]";
                if (BLOCKING_ATTEMPT_ERROR_THRESHOLD < attempts && attempts < BLOCKING_ATTEMPT_SUPPRESSION)
                {
                    // Repeated access failures implies the database itself is unhealthy.
                    SWSS_LOG_ERROR("%s", msg.c_str());
                }
                else
                {
                    SWSS_LOG_WARN("%s", msg.c_str());
                }
            }
        }
    }

    // Unsubscribe the chosent client from keyspace event notifications
    void _unsubscribe_keyspace_notification(int dbId)
    {
        auto found = keyspace_notification_channels.find(dbId);
        if (found != keyspace_notification_channels.end())
        {
            SWSS_LOG_DEBUG("Unsubscribe from keyspace notification");

            keyspace_notification_channels.erase(found);
        }
    }

    bool _unavailable_data_handler(int dbId, const char *data)
    {
        throw std::logic_error("Not implemented");
    }

    // Subscribe the chosent client to keyspace event notifications
    void _subscribe_keyspace_notification(int dbId)
    {
        SWSS_LOG_DEBUG("Subscribe to keyspace notification");
        auto& client = m_redisClient.at(dbId);
        DBConnector *pubsub = client.newConnector(0);
        pubsub->psubscribe(KEYSPACE_PATTERN);
        keyspace_notification_channels.emplace(std::piecewise_construct, std::forward_as_tuple(dbId), std::forward_as_tuple(pubsub));
    }

    // In the event Redis is unavailable, close existing connections, and try again.
    void _connection_error_handler(int dbId)
    {
        SWSS_LOG_WARN("Could not connect to Redis--waiting before trying again.");
        close(dbId);
        sleep(CONNECT_RETRY_WAIT_TIME);
        connect(dbId, true);
    }

    void _onetime_connect(int dbId)
    {
        m_redisClient.emplace(std::piecewise_construct
            , std::forward_as_tuple(dbId)
            , std::forward_as_tuple(dbId, *this));
    }

    // Keep reconnecting to Database 'db_id' until success
    void _persistent_connect(int dbId)
    {
        for (;;)
        {
            try
            {
                _onetime_connect(dbId);
            }
            catch (RedisError&)
            {
                const int wait = CONNECT_RETRY_WAIT_TIME;
                SWSS_LOG_WARN("Connecting to DB '%d' failed, will retry in %d s", dbId, wait);
                close(dbId);
                sleep(wait);
            }
        }
    }

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
