#pragma once

#include <string>
#include <functional>
#include "redisreply.h"
#include "rediscommand.h"
#include "dbconnector.h"

namespace swss {

class RedisPipeline {
public:
    RedisPipeline(DBConnector *db) : m_db(db), m_remaining(0) { }
    ~RedisPipeline() { flush(); }

    void push(const RedisCommand& command)
    {
        redisAppendFormattedCommand(m_db->getContext(), command.c_str(), command.length());
        m_remaining++;
    }
    
    void push(std::string command)
    {
        redisAppendCommand(m_db->getContext(), command.c_str());
        m_remaining++;
    }
    
    RedisReply pop()
    {
        redisReply *reply;
        redisGetReply(m_db->getContext(), (void**)&reply);
        m_remaining--;
        return reply;
    }
    
    void flush()
    {
        while(m_remaining)
        {
            m_remaining--;
            pop();
        }
    }
    
    size_t size()
    {
        return m_remaining;
    }
    
private:
    DBConnector *m_db;
    size_t m_remaining;
};

}
