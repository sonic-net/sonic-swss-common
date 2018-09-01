#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <utility>
#include <algorithm>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "redispipeline.h"
#include "producerstatetable.h"

using namespace std;

namespace swss {

ProducerStateTable::ProducerStateTable(DBConnector *db, const string &tableName)
    : ProducerStateTable(new RedisPipeline(db, 1), tableName, false)
{
    m_pipeowned = true;
}

ProducerStateTable::ProducerStateTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
    : TableBase(pipeline->getDbId(), tableName)
    , TableName_KeySet(tableName)
    , m_buffered(buffered)
    , m_pipeowned(false)
    , m_pipe(pipeline)
{
    string luaSet =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "for i = 0, #KEYS - 3 do\n"
        "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], ARGV[4 + i * 2])\n"
        "end\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";
    m_shaSet = m_pipe->loadRedisScript(luaSet);

    string luaDel =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";
    m_shaDel = m_pipe->loadRedisScript(luaDel);

    string luaDrop =
        "redis.call('DEL', KEYS[1])\n"
        "redis.call('DEL', KEYS[2])\n";
    m_shaDrop = m_pipe->loadRedisScript(luaDrop);
}

ProducerStateTable::~ProducerStateTable()
{
    if (m_pipeowned)
    {
        delete m_pipe;
    }
}

void ProducerStateTable::setBuffered(bool buffered)
{
    m_buffered = buffered;
}

void ProducerStateTable::set(const string &key, const vector<FieldValueTuple> &values,
                 const string &op /*= SET_COMMAND*/, const string &prefix)
{
    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaSet);
    args.emplace_back(to_string(values.size() + 2));
    args.emplace_back(getChannelName());
    args.emplace_back(getKeySetName());

    args.insert(args.end(), values.size(), getStateHashPrefix() + getKeyName(key));

    args.emplace_back("G");
    args.emplace_back(key);
    for (const auto& iv: values)
    {
        args.emplace_back(fvField(iv));
        args.emplace_back(fvValue(iv));
    }

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    m_pipe->push(command, REDIS_REPLY_NIL);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerStateTable::del(const string &key, const string &op /*= DEL_COMMAND*/, const string &prefix)
{
    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaDel);
    args.emplace_back("3");
    args.emplace_back(getChannelName());
    args.emplace_back(getKeySetName());
    args.emplace_back(getStateHashPrefix() + getKeyName(key));
    args.emplace_back("G");
    args.emplace_back(key);
    args.emplace_back("''");

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    m_pipe->push(command, REDIS_REPLY_NIL);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerStateTable::flush()
{
    m_pipe->flush();
}

int64_t ProducerStateTable::pendingCount()
{
    RedisCommand cmd;
    cmd.format("SCARD %s", getKeySetName().c_str());
    RedisReply r = m_pipe->push(cmd);
    r.checkReplyType(REDIS_REPLY_INTEGER);

    return r.getReply<long long int>();
}

// Warning: calling this function will cause all data in keyset and the temporary table to be abandoned.
// ConsumerState may have got the notification from PUBLISH, but will see no data popped.
void ProducerStateTable::drop()
{
    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaDrop);
    args.emplace_back("2");
    args.emplace_back(getKeySetName());
    args.emplace_back(getStateHashPrefix() + getTableName());
    args.emplace_back("G");
    args.emplace_back("''");

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand cmd;
    cmd.formatArgv((int)args1.size(), &args1[0], NULL);
    m_pipe->push(cmd, REDIS_REPLY_NIL);
    m_pipe->flush();
}

}
