#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <algorithm>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "redispipeline.h"
#include "producerstatetable.h"

using namespace std;

namespace swss {

ProducerStateTable::ProducerStateTable(DBConnector *db, string tableName)
    : TableName_KeySet(tableName)
    , m_db(db)
    , m_pipe(new RedisPipeline(db))
{
    std::string luaSet =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "for i = 0, #KEYS - 3 do\n"
        "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], ARGV[4 + i * 2])\n"
        "end\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";
    shaSet = loadRedisScript(m_db, luaSet);

    std::string luaDel =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";
    shaDel = loadRedisScript(m_db, luaDel);
}

ProducerStateTable::~ProducerStateTable()
{
    flush();
    delete m_pipe;
}

task ProducerStateTable::setAsync(std::string key, std::vector<FieldValueTuple> &values,
                 std::string op /*= SET_COMMAND*/)
{
    // Assembly redis command args into a string vector
    vector<string> args;
    args.push_back("EVALSHA");
    args.push_back(shaSet);
    args.push_back(to_string(values.size() + 2));
    args.push_back(getChannelName());
    args.push_back(getKeySetName());

    args.insert(args.end(), values.size(), getKeyName(key));

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
    m_pipe->push(command);
    return [&]{ return RedisReply(m_pipe->pop()).checkReplyType(REDIS_REPLY_NIL); };
}

void ProducerStateTable::set(std::string key, std::vector<FieldValueTuple> &values,
                 std::string op /*= SET_COMMAND*/, std::string prefix)
{
    setAsync(key, values, op)();
}

task ProducerStateTable::delAsync(std::string key, std::string op /*= DEL_COMMAND*/)
{
    // Assembly redis command args into a string vector
    vector<string> args;
    args.push_back("EVALSHA");
    args.push_back(shaDel);
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
    m_pipe->push(command);
    return [&]{ return RedisReply(m_pipe->pop()).checkReplyType(REDIS_REPLY_NIL); };
}

void ProducerStateTable::del(std::string key, std::string op /*= DEL_COMMAND*/, std::string prefix)
{
    delAsync(key)();
}

void ProducerStateTable::flush()
{
    while(m_pipe->size())
    {
        RedisReply r(m_pipe->pop());
        r.checkReplyType(REDIS_REPLY_NIL);
    }
}

}
