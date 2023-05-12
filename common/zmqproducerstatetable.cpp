#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <utility>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <zmq.h>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "redispipeline.h"
#include "zmqproducerstatetable.h"
#include "zmqconsumerstatetable.h"
#include "binaryserializer.h"

using namespace std;

namespace swss {

ZmqProducerStateTable::ZmqProducerStateTable(DBConnector *db, const string &tableName, ZmqClient &zmqClient)
    : ProducerStateTable(db, tableName),
    m_zmqClient(zmqClient),
    m_dbName(db->getDbName()),
    m_tableNameStr(tableName)
{
    initialize();
}

ZmqProducerStateTable::ZmqProducerStateTable(RedisPipeline *pipeline, const string &tableName, ZmqClient &zmqClient, bool buffered)
    : ProducerStateTable(pipeline, tableName, buffered),
    m_zmqClient(zmqClient),
    m_dbName(pipeline->getDbName()),
    m_tableNameStr(tableName)
{
    initialize();
}

void ZmqProducerStateTable::initialize()
{
    m_sendbuffer.resize(MQ_RESPONSE_MAX_COUNT);
}

void ZmqProducerStateTable::set(
                    const string &key,
                    const vector<FieldValueTuple> &values,
                    const string &op /*= SET_COMMAND*/,
                    const string &prefix)
{
    m_zmqClient.sendMsg(
                        key,
                        values,
                        op,
                        m_dbName,
                        m_tableNameStr,
                        m_sendbuffer);
}

void ZmqProducerStateTable::del(
                    const string &key,
                    const string &op /*= DEL_COMMAND*/,
                    const string &prefix)
{
    m_zmqClient.sendMsg(
                        key,
                        vector<FieldValueTuple>(),
                        op,
                        m_dbName,
                        m_tableNameStr,
                        m_sendbuffer);
}

void ZmqProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple> &values)
{
    for (const auto &value : values)
    {
        set(
            kfvKey(value),
            kfvFieldsValues(value),
            SET_COMMAND);
    }
}

void ZmqProducerStateTable::del(const std::vector<std::string> &keys)
{
    for (const auto &key : keys)
    {
        del(key, DEL_COMMAND);
    }
}

}
