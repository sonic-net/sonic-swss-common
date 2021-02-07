#include "redistran.h"
#include "dbconnector.h"

namespace swss {

RedisTransactioner::RedisTransactioner(DBConnector *db) : m_db(db)
{
}

RedisTransactioner::~RedisTransactioner()
{
    clearResults();
}

/* Start a transaction */
void RedisTransactioner::multi()
{
    m_expectedResults.clear();
    RedisReply r(m_db, "MULTI", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

/* Execute a transaction and get results */
bool RedisTransactioner::exec()
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
                "Got to different number of answers!");

    clearResults();
    for (unsigned int i = 0; i < size; i++)
    {
        int expectedType = m_expectedResults.front();
        m_expectedResults.pop_front();
        if (expectedType != reply->element[i]->type)
        {
            SWSS_LOG_ERROR("Expected to get redis type %d got type %d",
                    expectedType, reply->element[i]->type);
            throw system_error(make_error_code(errc::io_error),
                    "Got unexpected result");
        }
    }

    for (size_t i = 0; i < size; i++)
        m_results.push_back(reply->element[i]);

    /* Free only the array memory */
    r.release();
    free(reply->element);
    free(reply);
    return true;
}

/* Send a command within a transaction */
void RedisTransactioner::enqueue(const std::string& command, int expectedType)
{
    RedisReply r(m_db, command, REDIS_REPLY_STATUS);
    r.checkStatusQueued();
    m_expectedResults.push_back(expectedType);
}

void RedisTransactioner::enqueue(const RedisCommand& command, int expectedType)
{
    RedisReply r(m_db, command, REDIS_REPLY_STATUS);
    r.checkStatusQueued();
    m_expectedResults.push_back(expectedType);
}

redisReply *RedisTransactioner::dequeueReply()
{
    if (m_results.empty())
    {
        return NULL;
    }
    redisReply *ret = m_results.front();
    m_results.pop_front();
    return ret;
}

void RedisTransactioner::clearResults()
{
    if (m_results.empty())
    {
        return;
    }
    for (const auto& r: m_results)
    {
        freeReplyObject(r);
    }
    m_results.clear();
}

}
