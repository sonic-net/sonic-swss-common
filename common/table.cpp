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

std::string Table::getTableName()const
{
    return m_tableName;
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

bool Table::get(std::string key, vector<FieldValueTuple> &values)
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

    const std::string &cmd = formatHMSET(getKeyName(key), values);
    
    RedisReply r(m_db, cmd, REDIS_REPLY_STATUS, true);

    r.checkStatusOK();
}

void Table::del(std::string key, std::string /* op */)
{
    RedisReply r(m_db, string("DEL ") + getKeyName(key), REDIS_REPLY_INTEGER);
    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw system_error(make_error_code(errc::io_error),
                           "DEL operation failed");
}

bool Table::getField(std::string key, std::string field, std::string &value)
{
    std::string hget = formatHGET(getKeyName(key).c_str(),
                                  field.c_str());

    RedisReply r(m_db, hget, REDIS_REPLY_INTEGER);

    if (r.getContext()->type != REDIS_REPLY_STRING)
    {
        return false;
    }

    value = std::string(r.getContext()->str);

    return true;
}

void Table::setField(std::string key, std::string field, std::string value)
{
    FieldValueTuple entry(field, value);

    std::vector<FieldValueTuple> values { entry };

    set(key, values);
}

void Table::delField(std::string key, std::string field)
{
    std::string hdel = formatHDEL(getKeyName(key), field);

    RedisReply r(m_db, hdel, REDIS_REPLY_INTEGER);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw system_error(make_error_code(errc::io_error),
                           "DEL operation failed");
}

void Table::getTableContent(std::vector<KeyOpFieldsValuesTuple> &tuples)
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

Table::~Table()
{
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

string Table::formatHMSET(const std::string &key,
                          const std::vector<FieldValueTuple> &values)
{
    if (values.size() == 0)
        throw system_error(make_error_code(errc::io_error),
                           "HMSET must have some arguments");

    const char* cmd = "HMSET";

    std::vector<const char*> args = { cmd, key.c_str() };

    for (const auto &fvt: values)
    {
        args.push_back(fvField(fvt).c_str());
        args.push_back(fvValue(fvt).c_str());
    }

    char *temp;
    int len = redisFormatCommandArgv(&temp, args.size(), args.data(), NULL);
    string hmset(temp, len);
    free(temp);
    return hmset;
}

string Table::formatHSET(const string& key, const string& field,
                         const string& value)
{
        char *temp;
        int len = redisFormatCommand(&temp, "HSET %s %s %s",
                                     key.c_str(),
                                     field.c_str(),
                                     value.c_str());
        string hset(temp, len);
        free(temp);
        return hset;
}

string Table::formatHGET(const string& key, const string& field)
{
        char *temp;
        int len = redisFormatCommand(&temp, "HGET %s %s",
                                     key.c_str(),
                                     field.c_str());
        string hget(temp, len);
        free(temp);
        return hget;
}

string Table::formatHDEL(const string& key, const string& field)
{
        char *temp;
        int len = redisFormatCommand(&temp, "HDEL %s %s",
                                     key.c_str(),
                                     field.c_str());
        string hdel(temp, len);
        free(temp);
        return hdel;
}

}
