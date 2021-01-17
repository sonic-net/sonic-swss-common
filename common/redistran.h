#pragma once

#include <system_error>
#include <deque>
#include "dbconnector.h"
#include "rediscommand.h"
#include "logger.h"

namespace swss {

class RedisTransactioner {
public:
    RedisTransactioner(DBConnector *db);

    ~RedisTransactioner();

    /* Start a transaction */
    void multi();

    /* Execute a transaction and get results */
    bool exec();

    /* Send a command within a transaction */
    void enqueue(const std::string& command, int expectedType);
    void enqueue(const RedisCommand& command, int expectedType);

    redisReply *dequeueReply();

protected:
    DBConnector *m_db;
private:
    /* Remember the expected results for the transaction */
    std::deque<int> m_expectedResults;
    std::deque<redisReply *> m_results;

    void clearResults();
};

}
