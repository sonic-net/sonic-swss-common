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

ProducerTable::ProducerTable(DBConnector *db, const string &tableName)
    : ProducerTable(new RedisPipeline(db, 1), tableName, false)
{
    m_pipeowned = true;
}

ProducerTable::ProducerTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
    : TableBase(pipeline->getDbId(), tableName)
    , TableName_KeyValueOpQueues(tableName)
    , m_buffered(buffered)
    , m_pipeowned(false)
    , m_pipe(pipeline)
{
    /*
     * KEYS[1] : tableName + "_KEY_VALUE_OP_QUEUE
     * ARGV[1] : key
     * ARGV[2] : value
     * ARGV[3] : op
     * KEYS[2] : tableName + "_CHANNEL"
     * ARGV[4] : "G"
     */
    string luaEnque =
        "redis.call('LPUSH', KEYS[1], ARGV[1], ARGV[2], ARGV[3]);"
        "redis.call('PUBLISH', KEYS[2], ARGV[4]);";

    m_shaEnque = m_pipe->loadRedisScript(luaEnque);
}

ProducerTable::ProducerTable(DBConnector *db, const string &tableName, const string &dumpFile)
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

void ProducerTable::enqueueDbChange(const string &key, const string &value, const string &op, const string& /* prefix */)
{
    RedisCommand command;

    command.format(
        "EVALSHA %s 2 %s %s %s %s %s %s",
        m_shaEnque.c_str(),
        getKeyValueOpQueueTableName().c_str(),
        getChannelName().c_str(),
        key.c_str(),
        value.c_str(),
        op.c_str(),
        "G");

    m_pipe->push(command, REDIS_REPLY_NIL);
}

void ProducerTable::set(const string &key, const vector<FieldValueTuple> &values, const string &op, const string &prefix)
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
        for (const auto &it : values)
            j[json_key][fvField(it)] = fvValue(it);
        j["OP"] = op;
        m_dumpFile << j.dump(4);
    }

    enqueueDbChange(key, JSon::buildJson(values), "S" + op, prefix);
    // Only buffer "set", "bulkset" or "create" operations
    if (!m_buffered || (op != "create" && op != "set" && op != "bulkset" ))
    {
        m_pipe->flush();
    }
}

void ProducerTable::del(const string &key, const string &op, const string &prefix)
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
