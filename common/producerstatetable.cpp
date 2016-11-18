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
                 std::string op /*= SET_COMMAND*/, std::string prefix)
{
    static std::string luaScript =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "for i = 0, #KEYS - 3 do\n"
        "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], string.sub(ARGV[4 + i * 2], 2, -1))\n"
        "end\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";

    static std::string sha = loadRedisScript(m_db, luaScript);

    std::ostringstream osk, osv;
    osk << "EVALSHA "
        << sha << ' '
        << values.size() + 2 << ' '
        << getChannelName() << ' '
        << getKeySetName() << ' ';

    osv << "G "
        << key << ' ';

    for (auto& iv: values)
    {
        osk << getKeyName(key) << ' ';
        osv << fvField(iv) << ' '
            << encodeLuaArgument(fvValue(iv)) << ' ';
    }

    std::string command = osk.str() + osv.str();

    RedisReply r(m_db, command, REDIS_REPLY_NIL);
}

void ProducerStateTable::del(std::string key, std::string op /*= DEL_COMMAND*/, std::string prefix)
{
    static std::string luaScript =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";

    static std::string sha = loadRedisScript(m_db, luaScript);

    std::ostringstream osk, osv;
    osk << "EVALSHA "
        << sha << ' '
        << 3 << ' '
        << getChannelName() << ' '
        << getKeySetName() << ' '
        << getKeyName(key) << ' ';

    osv << "G "
        << key << ' '
        << "''";

    std::string command = osk.str() + osv.str();

    RedisReply r(m_db, command, REDIS_REPLY_NIL);
}

}

