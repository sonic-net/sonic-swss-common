#pragma once

#include <stdlib.h>
#include <tuple>
#include <sstream>
#include "redisreply.h"
#include "redisselect.h"
#include "json.h"
#include "json.hpp"
#include "table.h"

using json = nlohmann::json;

namespace swss {

class ProducerStateTable : public RedisChannel, public TableName_KeySet, public TableEntryWritable
{
public:
    ProducerStateTable(DBConnector *db, std::string tableName)
        : RedisChannel(db, tableName)
        , TableName_KeySet(tableName)
    {
    }

    /* Implements set() and del() commands using notification messages */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND)
    {
        static std::string luaScript =
            "redis.call('PUBLISH', KEYS[1], ARGV[1])\n"
            "redis.call('SADD', KEYS[2], ARGV[2])\n"
            "for i = 0, #KEYS - 3 do\n"
            "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], ARGV[4 + i * 2])\n"
            "end\n";

        static std::string sha = scriptLoad(luaScript);

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
                << fvValue(iv) << ' ';
        }

        std::string command = osk.str() + osv.str();

        RedisReply r(m_db, command, REDIS_REPLY_NIL, false);
    }

    virtual void del(std::string key, std::string op = DEL_COMMAND)
    {
        static std::string luaScript =
            "redis.call('PUBLISH', KEYS[1], ARGV[1])\n"
            "redis.call('SADD', KEYS[2], ARGV[2])\n"
            "redis.call('DEL', KEYS[3])\n";

        static std::string sha = scriptLoad(luaScript);

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

        RedisReply r(m_db, command, REDIS_REPLY_NIL, false);
    }
};

}

