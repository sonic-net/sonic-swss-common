#pragma once

#include <string>
#include <functional>
#include "redisreply.h"
#include "rediscommand.h"
#include "dbconnector.h"

namespace swss {

class RedisPipeline {
public:
    const size_t COMMAND_MAX;

    RedisPipeline(DBConnector *db)
        : COMMAND_MAX(128)
        , m_db(db)
        , m_remaining(0)
    {
    }

    ~RedisPipeline() {
        flush();
    }

    redisReply *push(const RedisCommand& command, int expectedType)
    {
        if (expectedType == REDIS_REPLY_NIL)
        {
            redisAppendFormattedCommand(m_db->getContext(), command.c_str(), command.length());
            m_remaining++;
            mayflush();
            return NULL;
        }
        else
        {
            flush();
            RedisReply r(m_db, command, expectedType);
            return r.release();
        }
    }

    std::string loadRedisScript(const std::string& script)
    {
        RedisCommand loadcmd;
        loadcmd.format("SCRIPT LOAD %s", script.c_str());
        RedisReply r = push(loadcmd, REDIS_REPLY_STRING);

        std::string sha = r.getReply<std::string>();
        return sha;
    }

    // The caller is reponsible to release the reply object
    redisReply *pop()
    {
        if (m_remaining == 0) return NULL;

        redisReply *reply;
        redisGetReply(m_db->getContext(), (void**)&reply);
        m_remaining--;
        return reply;
    }

    void flush()
    {
        while(m_remaining)
        {
            // Construct an object to use its dtor, so that resource is released
            RedisReply r(pop());
        }
    }

    size_t size()
    {
        return m_remaining;
    }

private:
    DBConnector *m_db;
    size_t m_remaining;

    void mayflush()
    {
        if (m_remaining > COMMAND_MAX)
            flush();
    }
};

}
