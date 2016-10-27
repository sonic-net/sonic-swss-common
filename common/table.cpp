#include <hiredis/hiredis.h>
#include <system_error>

#include "common/table.h"
#include "common/logger.h"
#include "common/redisreply.h"
#include "common/rediscommand.h"

using namespace std;

namespace swss {

Table::Table(DBConnector *db, string tableName) : RedisTransactioner(db), TableBase(tableName)
{
}

bool Table::get(std::string key, std::vector<FieldValueTuple> &values)
{
    string hgetall_key("HGETALL ");
    hgetall_key += getKeyName(key);

    RedisReply r(m_db, hgetall_key, REDIS_REPLY_ARRAY);
    redisReply *reply = r.getContext();
    values.clear();

    if (!reply->elements)
        return false;

    if (reply->elements & 1)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect netlink socket");

    for (unsigned int i = 0; i < reply->elements; i += 2)
        values.push_back(make_tuple(reply->element[i]->str,
                                    reply->element[i + 1]->str));

    return true;
}

void Table::set(std::string key, std::vector<FieldValueTuple> &values,
                std::string /*op*/)
{
    if (values.size() == 0)
        return;

    RedisCommand cmd;
    cmd.formatHMSET(getKeyName(key), values);

    RedisReply r(m_db, cmd, REDIS_REPLY_STATUS);

    r.checkStatusOK();
}

void Table::del(std::string key, std::string /* op */)
{
    RedisReply r(m_db, string("DEL ") + getKeyName(key), REDIS_REPLY_INTEGER);
}

void TableEntryEnumerable::getTableContent(std::vector<KeyOpFieldsValuesTuple> &tuples)
{
    std::vector<std::string> keys;
    getTableKeys(keys);

    tuples.clear();

    for (auto key: keys)
    {
        std::vector<FieldValueTuple> values;
        std::string op = "";

        get(key, values);
        tuples.push_back(make_tuple(key, op, values));
    }
}

void Table::getTableKeys(std::vector<std::string> &keys)
{
    string keys_cmd("KEYS " + getTableName() + ":*");
    RedisReply r(m_db, keys_cmd, REDIS_REPLY_ARRAY);
    redisReply *reply = r.getContext();
    keys.clear();

    for (unsigned int i = 0; i < reply->elements; i++)
    {
        string key = reply->element[i]->str;
        auto pos = key.find(':');
        keys.push_back(key.substr(pos+1));
    }
}

void RedisTransactioner::multi()
{
    while (!m_expectedResults.empty())
        m_expectedResults.pop();
    RedisReply r(m_db, "MULTI", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

redisReply *RedisTransactioner::queueResultsFront()
{
    return m_results.front()->getContext();
}

string RedisTransactioner::queueResultsPop()
{
    string ret(m_results.front()->getReply<string>());
    delete m_results.front();
    m_results.pop();
    return ret;
}

void RedisTransactioner::exec()
{
    redisReply *reply = (redisReply *)redisCommand(m_db->getContext(), "EXEC");
    size_t size = reply->elements;

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
                SWSS_LOG_ERROR("Expected to get redis type %d got type %d",
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

    for (size_t i = 0; i < size; i++)
        /* FIXME: not enough memory */
        m_results.push(new RedisReply(reply->element[i]));

    /* Free only the array memory */
    free(reply->element);
    free(reply);
}

void RedisTransactioner::enqueue(std::string command, int expectedType)
{
    RedisReply r(m_db, command, REDIS_REPLY_STATUS);
    r.checkStatusQueued();
    m_expectedResults.push(expectedType);
}

}
