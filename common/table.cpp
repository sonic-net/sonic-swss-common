#include <hiredis/hiredis.h>
#include <system_error>

#include "common/table.h"
#include "common/logger.h"
#include "common/redisreply.h"

using namespace std;

namespace swss {

Table::Table(DBConnector *db, string tableName) :
    m_db(db),
    m_tableName(tableName)
{
}

string Table::getKeyName(string key)
{
    return m_tableName + ':' + key;
}

string Table::getKeyQueueTableName()
{
    return m_tableName + "_KEY_QUEUE";
}

string Table::getValueQueueTableName()
{
    return m_tableName + "_VALUE_QUEUE";
}

string Table::getOpQueueTableName()
{
    return m_tableName + "_OP_QUEUE";
}

string Table::getChannelTableName()
{
    return m_tableName + "_CHANNEL";
}

void Table::multi()
{
    while (!m_expectedResults.empty())
        m_expectedResults.pop();
    RedisReply r(m_db, "MULTI", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

redisReply *Table::queueResultsFront()
{
    return m_results.front()->getContext();
}

void Table::queueResultsPop()
{
    delete m_results.front();
    m_results.pop();
}

void Table::exec()
{
    redisReply *reply = (redisReply *)redisCommand(m_db->getContext(), "EXEC");
    unsigned int size = reply->elements;

    try
    {
        if (reply->type != REDIS_REPLY_ARRAY)
            throw system_error(make_error_code(errc::io_error),
                               "Error in transaction");

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
                SWSS_LOG_INFO("Except to get redis type %d got type %d\n",
                              expectedType, reply->element[i]->type);
                throw system_error(make_error_code(errc::io_error),
                                   "Got unexpected result");
            }
        }
    }
    catch (...)  {
        freeReplyObject(reply);
        throw;
    }

    for (unsigned int i = 0; i < size; i++)
        m_results.push(new RedisReply(reply->element[i]));

    /* Free only the array memory */
    free(reply->element);
    free(reply);
}

void Table::enqueue(std::string command, int exepectedResult, bool isFormatted)
{
    RedisReply r(m_db, command, REDIS_REPLY_STATUS, isFormatted);
    r.checkStatusQueued();
    m_expectedResults.push(exepectedResult);
}

}
