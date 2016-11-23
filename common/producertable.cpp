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
    : TableName_KeyValueOpQueues(tableName)
    , m_db(db)
{
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
}

void ProducerTable::enqueueDbChange(string key, string value, string op, string prefix)
{
    static string luaScript =
        "redis.call('LPUSH', KEYS[1], ARGV[1]);"
        "redis.call('LPUSH', KEYS[2], ARGV[2]);"
        "redis.call('LPUSH', KEYS[3], ARGV[3]);"
        "redis.call('PUBLISH', KEYS[4], ARGV[4]);";

    static string sha = loadRedisScript(m_db, luaScript);

    RedisCommand command;
    command.format(
        "EVALSHA %s 4 %s %s %s %s %s %s %s %s",
        sha.c_str(),
        (prefix+getKeyQueueTableName()).c_str(),
        (prefix+getValueQueueTableName()).c_str(),
        (prefix+getOpQueueTableName()).c_str(),
        getChannelName().c_str(),
        key.c_str(),
        value.c_str(),
        op.c_str(),
        "G");

    RedisReply r(m_db, command, REDIS_REPLY_NIL);
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
}

}
