#include <stdlib.h>
#include <tuple>
#include <sstream>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "producerstatetable.h"

namespace swss {

ProducerStateTable::ProducerStateTable(DBConnector *db, std::string tableName)
    : TableName_KeySet(tableName)
    , m_db(db)
{
}

void ProducerStateTable::set(std::string key, std::vector<FieldValueTuple> &values,
                 std::string op /*= SET_COMMAND*/)
{
    static std::string luaScript =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('HINCRBY', KEYS[3], 'set', 1)\n"
        "for i = 0, #KEYS - 4 do\n"
        "    redis.call('HSET', KEYS[4 + i], ARGV[4 + i * 2], string.sub(ARGV[5 + i * 2], 2, -1))\n"
        "end\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";

    static std::string sha = loadRedisScript(m_db, luaScript);

    std::ostringstream osk, osv;
    osk << "EVALSHA "
        << sha << ' '
        << values.size() + 3 << ' '
        << getChannelName() << ' '
        << getKeySetName() << ' '
        << getCounterName() << ' ';

    osv << "G "
        << key
        << " '' ";

    for (auto& iv: values)
    {
        osk << getKeyName(key) << ' ';
        osv << fvField(iv) << ' '
            << encodeLuaArgument(fvValue(iv)) << ' ';
    }

    std::string command = osk.str() + osv.str();

    RedisReply r(m_db, command, REDIS_REPLY_NIL);
}

void ProducerStateTable::del(std::string key, std::string op /*= DEL_COMMAND*/)
{
    static std::string luaScript =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n"
        "redis.call('HINCRBY', KEYS[4], 'del', 1)\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";

    static std::string sha = loadRedisScript(m_db, luaScript);

    std::ostringstream osk, osv;
    osk << "EVALSHA "
        << sha << ' '
        << 4 << ' '
        << getChannelName() << ' '
        << getKeySetName() << ' '
        << getKeyName(key) << ' ' << getCounterName() << ' ';

    osv << "G "
        << key
        << " '' '' ";

    std::string command = osk.str() + osv.str();

    RedisReply r(m_db, command, REDIS_REPLY_NIL);
}

}

