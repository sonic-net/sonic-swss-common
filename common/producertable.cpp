#include "common/redisreply.h"
#include "common/producertable.h"
#include "common/json.h"
#include "common/json.hpp"
#include <stdlib.h>
#include <tuple>

using namespace std;
using json = nlohmann::json;

namespace swss {

ProducerTable::ProducerTable(DBConnector *db, string tableName) :
    Table(db, tableName)
{
}

ProducerTable::ProducerTable(DBConnector *db, string tableName, string dumpFile) :
    Table(db, tableName)
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

void ProducerTable::enqueueDbChange(string key, string value, string op)
{
    string lpush_value;
    string lpush_key("LPUSH ");
    string lpush_op = lpush_key;
    char *temp;
    int len;

    lpush_key += getKeyQueueTableName();
    lpush_key += " ";
    lpush_key += key;
    enqueue(lpush_key, REDIS_REPLY_INTEGER);

    len = redisFormatCommand(&temp, "LPUSH %s %s", getValueQueueTableName().c_str(), value.c_str());
    lpush_value = string(temp, len);
    free(temp);
    enqueue(lpush_value, REDIS_REPLY_INTEGER, true);

    lpush_op += getOpQueueTableName();
    lpush_op += " ";
    lpush_op += op;
    enqueue(lpush_op, REDIS_REPLY_INTEGER);

    string publish("PUBLISH ");
    publish += getChannelTableName();
    publish += " G";
    enqueue(publish, REDIS_REPLY_INTEGER);
}

void ProducerTable::set(string key, vector<FieldValueTuple> &values, string op)
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

    multi();
    enqueueDbChange(key, JSon::buildJson(values), "S" + op);
    exec();
}

void ProducerTable::del(string key, string op)
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

    multi();
    enqueueDbChange(key, "{}", "D" + op);
    exec();
}

}
