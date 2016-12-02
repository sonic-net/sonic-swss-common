#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <algorithm>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "producerstatetable.h"

using namespace std;

namespace swss {

ProducerStateTable::ProducerStateTable(DBConnector *db, string tableName)
    : TableName_KeySet(tableName)
    , m_db(db)
{
}

void ProducerStateTable::set(string key, vector<FieldValueTuple> &values,
                 string op /*= SET_COMMAND*/, string prefix)
{
    static string luaScript =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "for i = 0, #KEYS - 3 do\n"
        "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], ARGV[4 + i * 2])\n"
        "end\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";

    static string sha = loadRedisScript(m_db, luaScript);

    // Assembly redis command args into a string vector
    vector<string> args;
    args.push_back("EVALSHA");
    args.push_back(sha);
    args.push_back(to_string(values.size() + 2));
    args.push_back(getChannelName());
    args.push_back(getKeySetName());
    for (auto& iv: values)
    {
        args.push_back(getKeyName(key));
    }
    args.push_back("G");
    args.push_back(key);
    for (auto& iv: values)
    {
        args.push_back(fvField(iv));
        args.push_back(fvValue(iv));
    }

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string s) { return s.c_str(); } ); 
    
    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    RedisReply r(m_db, command, REDIS_REPLY_NIL);
}

void ProducerStateTable::del(string key, string op /*= DEL_COMMAND*/, string prefix)
{
    static string luaScript =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";

    static string sha = loadRedisScript(m_db, luaScript);
    
    // Assembly redis command args into a string vector
    vector<string> args;
    args.push_back("EVALSHA");
    args.push_back(sha);
    args.push_back("3");
    args.push_back(getChannelName());
    args.push_back(getKeySetName());
    args.push_back(getKeyName(key));
    args.push_back("G");
    args.push_back(key);
    args.push_back("''");

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string s) { return s.c_str(); } ); 
    
    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    RedisReply r(m_db, command, REDIS_REPLY_NIL);
}

}

