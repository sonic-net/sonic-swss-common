#pragma once

#include <system_error>
#include "redisreply.h"
#include "rediscommand.h"
#include "logger.h"

namespace swss {

class RedisTransactioner {
public:
    RedisTransactioner(DBConnector *db) : m_db(db) { }
    
    /* Start a transaction */
    void multi()
    {
        while (!m_expectedResults.empty())
            m_expectedResults.pop();
        RedisReply r(m_db, "MULTI", REDIS_REPLY_STATUS);
        r.checkStatusOK();
    }
    
    /* Execute a transaction and get results */
    bool exec()
    {
        using namespace std;
        
        RedisReply r(m_db, "EXEC");
        redisReply *reply = r.getContext();
        size_t size = reply->elements;

        // if meet error in transaction
        if (reply->type != REDIS_REPLY_ARRAY)
        {
            return false;
        }

        if (size != m_expectedResults.size())
            throw system_error(make_error_code(errc::io_error),
                               "Got to different nuber of answers!");

        while (!m_results.empty())
            queueResultsPop();

        for (unsigned int i = 0; i < size; i++)
        {
            int expectedType = m_expectedResults.front();
            m_expectedResults.pop();
            if (expectedType != reply->element[i]->type)
            {
                SWSS_LOG_ERROR("Expected to get redis type %d got type %d",
                              expectedType, reply->element[i]->type);
                throw system_error(make_error_code(errc::io_error),
                                   "Got unexpected result");
            }
        }

        for (size_t i = 0; i < size; i++)
            /* FIXME: not enough memory */
            m_results.push(new RedisReply(reply->element[i]));

        /* Free only the array memory */
        r.release();
        free(reply->element);
        free(reply);
        return true;
    }
    
    /* Send a command within a transaction */
    void enqueue(std::string command, int expectedType)
    {
        RedisReply r(m_db, command, REDIS_REPLY_STATUS);
        r.checkStatusQueued();
        m_expectedResults.push(expectedType);
    }
    
    redisReply* queueResultsFront() { return m_results.front()->getContext(); }
    std::string queueResultsPop()
    {
        std::string ret(m_results.front()->getReply<std::string>());
        delete m_results.front();
        m_results.pop();
        return ret;
    }
    
protected:
    DBConnector *m_db;
private:    
    /* Remember the expected results for the transaction */
    std::queue<int> m_expectedResults;
    std::queue<RedisReply *> m_results;
};

}
