#pragma once

#include <string>
#include <queue>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <iostream>
#include "redisreply.h"
#include "rediscommand.h"
#include "dbconnector.h"
#include "logger.h"

#include "unistd.h"
#include "sys/syscall.h"
#define gettid() syscall(SYS_gettid)

namespace swss {

class RedisPipeline {
public:
    const size_t COMMAND_MAX;
    static constexpr int NEWCONNECTOR_TIMEOUT = 0;

    RedisPipeline(const DBConnector *db, size_t sz = 128)
        : COMMAND_MAX(sz)
        , m_remaining(0)
        , m_shaPub("")
    {
        m_db = db->newConnector(NEWCONNECTOR_TIMEOUT);
        initializeOwnerTid();
        lastHeartBeat = std::chrono::steady_clock::now();
    }

    ~RedisPipeline() {
        if (m_ownerTid == gettid())
        {
            // call flush from different thread will trigger race condition issue.
            flush();
        }
        else
        {
            SWSS_LOG_NOTICE("RedisPipeline dtor is called from another thread, possibly due to exit(), Database: %s", getDbName().c_str());
        }

        delete m_db;
    }

    redisReply *push(const RedisCommand& command, int expectedType)
    {
        switch (expectedType)
        {
            case REDIS_REPLY_NIL:
            case REDIS_REPLY_STATUS:
            case REDIS_REPLY_INTEGER:
            {
                int rc = command.appendTo(m_db->getContext());
                if (rc != REDIS_OK)
                {
                    // The only reason of error is REDIS_ERR_OOM (Out of memory)
                    // ref: https://github.com/redis/hiredis/blob/master/hiredis.c
                    throw std::bad_alloc();
                }
                m_expectedTypes.push(expectedType);
                m_remaining++;
                mayflush();
                return NULL;
            }
            default:
            {
                flush();
                RedisReply r(m_db, command, expectedType);
                return r.release();
            }
        }
    }

    redisReply *push(const RedisCommand& command)
    {
        flush();
        RedisReply r(m_db, command);
        return r.release();
    }

    std::string loadRedisScript(const std::string& script)
    {
        RedisCommand loadcmd;
        loadcmd.format("SCRIPT LOAD %s", script.c_str());
        RedisReply r = push(loadcmd, REDIS_REPLY_STRING);

        std::string sha = r.getReply<std::string>();
        return sha;
    }

    // The caller is responsible to release the reply object
    redisReply *pop()
    {
        if (m_remaining == 0) return NULL;

        redisReply *reply;
        int rc = redisGetReply(m_db->getContext(), (void**)&reply);
        if (rc != REDIS_OK)
        {
            throw RedisError("Failed to redisGetReply in RedisPipeline::pop", m_db->getContext());
        }
        RedisReply r(reply);
        m_remaining--;

        int expectedType = m_expectedTypes.front();
        m_expectedTypes.pop();
        r.checkReplyType(expectedType);
        if (expectedType == REDIS_REPLY_STATUS)
        {
            r.checkStatusOK();
        }
        return r.release();
    }

    void flush()
    {
        lastHeartBeat = std::chrono::steady_clock::now();

        if (m_remaining == 0) {
            return;
        }

        while(m_remaining)
        {
            // Construct an object to use its dtor, so that resource is released
            RedisReply r(pop());
        }

        publish();
    }

    size_t size()
    {
        return m_remaining;
    }

    int getDbId()
    {
        return m_db->getDbId();
    }

    std::string getDbName()
    {
        return m_db->getDbName();
    }

    DBConnector *getDBConnector()
    {
        return m_db;
    }

    void initializeOwnerTid()
    {
        m_ownerTid = gettid();
    }

    void addChannel(std::string channel)
    {
        if (m_channels.find(channel) != m_channels.end())
            return;

        m_channels.insert(channel);
        m_luaPub += "redis.call('PUBLISH', '" + channel + "', 'G');";
        m_shaPub = loadRedisScript(m_luaPub);
    }

    int getIdleTime(std::chrono::time_point<std::chrono::steady_clock> tcurrent=std::chrono::steady_clock::now())
    {
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(tcurrent - lastHeartBeat).count());
    }

    void publish() {
        if (m_shaPub.empty()) {
            return;
        }
        RedisCommand cmd;
        cmd.format(
            "EVALSHA %s 0",
            m_shaPub.c_str());
        RedisReply r(m_db, cmd);
    }

private:
    DBConnector *m_db;
    std::queue<int> m_expectedTypes;
    size_t m_remaining;
    long int m_ownerTid;

    std::string m_luaPub;
    std::string m_shaPub;
    std::chrono::time_point<std::chrono::steady_clock> lastHeartBeat; // marks the timestamp of latest pipeline flush being invoked
    std::unordered_set<std::string> m_channels;

    void mayflush()
    {
        if (m_remaining >= COMMAND_MAX)
            flush();
    }
};

}
