#include <hiredis/hiredis.h>
#include <system_error>

#include "common/table.h"
#include "common/logger.h"
#include "common/redisreply.h"
#include "common/rediscommand.h"

using namespace std;
using namespace swss;

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
