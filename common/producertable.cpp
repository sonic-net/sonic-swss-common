#include <stdlib.h>
#include <tuple>
#include "common/redisreply.h"
#include "common/producertable.h"
#include "common/json.h"
#include "common/json.hpp"
#include "common/logger.h"
#include "common/redisapi.h"

using namespace std;
using json = nlohmann::json;

namespace swss {

ProducerTable::ProducerTable(DBConnector *db, string tableName)
    : ProducerTable(new RedisPipeline(db, 1), tableName, false)
{
    m_pipeowned = true;
}

ProducerTable::ProducerTable(RedisPipeline *pipeline, string tableName, bool buffered)
    : TableBase(tableName)
    , TableName_KeyValueOpQueues(tableName)
    , m_buffered(buffered)
    , m_pipeowned(false)
    , m_pipe(pipeline)
{
    string luaEnque =
        "redis.call('LPUSH', KEYS[1], ARGV[1]);"
        "redis.call('LPUSH', KEYS[2], ARGV[2]);"
        "redis.call('LPUSH', KEYS[3], ARGV[3]);"
        "redis.call('PUBLISH', KEYS[4], ARGV[4]);";

    m_shaEnque = m_pipe->loadRedisScript(luaEnque);
}

ProducerTable::ProducerTable(DBConnector *db, string tableName, string dumpFile)
    : ProducerTable(db, tableName)
{
    m_dumpFile.open(dumpFile, fstream::out | fstream::trunc);
    m_dumpFile << "[" << endl;
}

ProducerTable::~ProducerTable() {
    if (m_dumpFile.is_open())
    {
        m_dumpFile << endl << "]" << endl;
        m_dumpFile.close();
    }

    if (m_pipeowned)
    {
        delete m_pipe;
    }
}

void ProducerTable::setBuffered(bool buffered)
{
    m_buffered = buffered;
}

void ProducerTable::enqueueDbChange(string key, string value, string op, string /* prefix */)
{
    RedisCommand command;
    command.format(
        "EVALSHA %s 4 %s %s %s %s %s %s %s %s",
        m_shaEnque.c_str(),
        getKeyQueueTableName().c_str(),
        getValueQueueTableName().c_str(),
        getOpQueueTableName().c_str(),
        getChannelName().c_str(),
        key.c_str(),
        value.c_str(),
        op.c_str(),
        "G");

    m_pipe->push(command, REDIS_REPLY_NIL);
}

void ProducerTable::set(string key, vector<FieldValueTuple> &values, string op, string prefix)
{
    if (m_dumpFile.is_open())
    {
        if (!m_firstItem)
            m_dumpFile << "," << endl;
        else
            m_firstItem = false;

        json j;
        string json_key = getKeyName(key);
        j[json_key] = json::object();
        for (auto it : values)
            j[json_key][fvField(it)] = fvValue(it);
        j["OP"] = op;
        m_dumpFile << j.dump(4);
    }

    enqueueDbChange(key, JSon::buildJson(values), "S" + op, prefix);
    // Only buffer continuous "set/set" or "del" operations
    if (!m_buffered || (op != "set" && op != "bulkset" ))
    {
        m_pipe->flush();
    }
}

void ProducerTable::del(string key, string op, string prefix)
{
    if (m_dumpFile.is_open())
    {
        if (!m_firstItem)
            m_dumpFile << "," << endl;
        else
            m_firstItem = false;

        json j;
        string json_key = getKeyName(key);
        j[json_key] = json::object();
        j["OP"] = op;
        m_dumpFile << j.dump(4);
    }

    enqueueDbChange(key, "{}", "D" + op, prefix);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerTable::flush()
{
    m_pipe->flush();
}

}
