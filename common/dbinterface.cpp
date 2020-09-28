#include <chrono>
#include <hiredis/hiredis.h>
#include "dbinterface.h"

using namespace std;
using namespace std::chrono;
using namespace swss;

void DBInterface::set_redis_kwargs(std::string unix_socket_path, std::string host, int port)
{
    m_unix_socket_path = unix_socket_path;
    m_host = host;
    m_port = port;
}

void DBInterface::connect(int dbId, bool retry)
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

void DBInterface::close(int dbId)
{
    m_redisClient.erase(dbId);
}

int64_t DBInterface::del(int dbId, const std::string& key, bool blocking)
{
    auto innerfunc = [&]
    {
        return m_redisClient.at(dbId).del(key);
    };
    return blockable<int64_t>(innerfunc, dbId, blocking);
}

bool DBInterface::exists(int dbId, const std::string& key)
{
    return m_redisClient.at(dbId).exists(key);
}

std::string DBInterface::get(int dbId, const std::string& hash, const std::string& key, bool blocking)
{
    auto innerfunc = [&]
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
    };
    return blockable<std::string>(innerfunc, dbId, blocking);
}

std::map<std::string, std::string> DBInterface::get_all(int dbId, const std::string& hash, bool blocking)
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
    return blockable<std::map<std::string, std::string>>(innerfunc, dbId, blocking);
}

std::vector<std::string> DBInterface::keys(int dbId, const std::string& pattern, bool blocking)
{
    auto innerfunc = [&]
    {
        auto keys = m_redisClient.at(dbId).keys(pattern);
        if (keys.empty())
        {
            std::string message = "DB '{" + std::to_string(dbId) + "}' is empty!";
            SWSS_LOG_WARN("%s", message.c_str());
            throw UnavailableDataError(message, "hset");
        }
        return keys;
    };
    return blockable<std::vector<std::string>>(innerfunc, dbId, blocking);
}

int64_t DBInterface::publish(int dbId, const std::string& channel, const std::string& message)
{
    return m_redisClient.at(dbId).publish(channel, message);
}

int64_t DBInterface::set(int dbId, const std::string& hash, const std::string& key, const std::string& value, bool blocking)
{
    auto innerfunc = [&]
    {
        m_redisClient.at(dbId).hset(hash, key, value);
        // Return the number of fields that were added.
        return 1;
    };
    return blockable<int64_t>(innerfunc, dbId, blocking);
}

DBConnector& DBInterface::get_redis_client(int dbId)
{
    return m_redisClient.at(dbId);
}

template <typename T, typename FUNC>
T DBInterface::blockable(FUNC f, int dbId, bool blocking)
{
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
void DBInterface::_unsubscribe_keyspace_notification(int dbId)
{
    auto found = keyspace_notification_channels.find(dbId);
    if (found != keyspace_notification_channels.end())
    {
        SWSS_LOG_DEBUG("Unsubscribe from keyspace notification");

        keyspace_notification_channels.erase(found);
    }
}

// When the queried config is not available in Redis--wait until it is available.
// Two timeouts are at work here:
// 1. Notification timeout - how long to wait before giving up on receiving any given pub-sub message.
// 2. Max data wait - swsssdk-specific. how long to wait for the data to populate (in absolute time)
bool DBInterface::_unavailable_data_handler(int dbId, const char *data)
{
    auto start = system_clock::now();
    SWSS_LOG_DEBUG("Listening on pubsub channel '%d'", dbId);
    auto wait = duration<float>(PUB_SUB_MAXIMUM_DATA_WAIT);
    while (system_clock::now() - start < wait)
    {
        auto& channel = keyspace_notification_channels.at(dbId);
        auto ctx = channel->getContext();
        redisReply *reply;
        int rc = redisGetReply(ctx, reinterpret_cast<void**>(&reply));
        if (rc == REDIS_ERR && ctx->err == REDIS_ERR_IO && errno == EAGAIN)
        {
            // Timeout
            continue;
        }
        if (rc != REDIS_OK)
        {
            throw RedisError("Failed to redisGetReply with on pubsub channel on dbId=" + to_string(dbId), ctx);
        }

        RedisReply r(reply);
        // r is an array of:
        //   0. 'type': 'pmessage',
        //   1. 'pattern': '__key*__:*'
        //   2. 'channel':
        //   3. 'data':
        redisReply& r3 = *r.getChild(3);
        if (r3.type != REDIS_REPLY_STRING)
        {
            throw system_error(make_error_code(errc::io_error),
                               "Wrong expected type of result");
        }

        if (strcmp(r3.str, data) == 0)
        {
            SWSS_LOG_INFO("'%s' acquired via pub-sub dbId=%d. Unblocking...", data, dbId);
            // Wait for a "settling" period before releasing the wait.
            sleep(DATA_RETRIEVAL_WAIT_TIME);
            return true;
        }
    }

    SWSS_LOG_WARN("No notification for '%s' from '%d' received before timeout.", data, dbId);
    return false;
}

// Subscribe the chosent client to keyspace event notifications
void DBInterface::_subscribe_keyspace_notification(int dbId)
{
    SWSS_LOG_DEBUG("Subscribe to keyspace notification");
    auto& client = m_redisClient.at(dbId);
    DBConnector *pubsub = client.newConnector(0);
    pubsub->psubscribe(KEYSPACE_PATTERN);

    // Set the timeout of the pubsub channel, so future redisGetReply will be impacted
    struct timeval tv = { 0, (suseconds_t)(1000 * PUB_SUB_NOTIFICATION_TIMEOUT) };
    int rc = redisSetTimeout(pubsub->getContext(), tv);
    if (rc != REDIS_OK)
    {
        throw RedisError("Failed to redisSetTimeout", pubsub->getContext());
    }

    keyspace_notification_channels.emplace(std::piecewise_construct, std::forward_as_tuple(dbId), std::forward_as_tuple(pubsub));
}

// In the event Redis is unavailable, close existing connections, and try again.
void DBInterface::_connection_error_handler(int dbId)
{
    SWSS_LOG_WARN("Could not connect to Redis--waiting before trying again.");
    close(dbId);
    sleep(CONNECT_RETRY_WAIT_TIME);
    connect(dbId, true);
}

void DBInterface::_onetime_connect(int dbId)
{
    if (m_unix_socket_path.empty())
    {
        m_redisClient.emplace(std::piecewise_construct
                , std::forward_as_tuple(dbId)
                , std::forward_as_tuple(dbId, m_host, m_port, 0));
    }
    else
    {
        m_redisClient.emplace(std::piecewise_construct
                , std::forward_as_tuple(dbId)
                , std::forward_as_tuple(dbId, m_unix_socket_path, 0));
    }

}

// Keep reconnecting to Database 'dbId' until success
void DBInterface::_persistent_connect(int dbId)
{
    for (;;)
    {
        try
        {
            _onetime_connect(dbId);
            return;
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
