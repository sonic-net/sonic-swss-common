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

ZmqProducerStateTable::ZmqProducerStateTable(DBConnector *db, const string &tableName, ZmqClient &zmqClient, bool dbPersistence)
    : ProducerStateTable(db, tableName),
    m_zmqClient(zmqClient),
    m_dbName(db->getDbName()),
    m_tableNameStr(tableName)
{
    initialize(db, tableName, dbPersistence);
}

ZmqProducerStateTable::ZmqProducerStateTable(RedisPipeline *pipeline, const string &tableName, ZmqClient &zmqClient, bool buffered, bool dbPersistence)
    : ProducerStateTable(pipeline, tableName, buffered),
    m_zmqClient(zmqClient),
    m_dbName(pipeline->getDbName()),
    m_tableNameStr(tableName)
{
    initialize(pipeline->getDBConnector(), tableName, dbPersistence);
}

void ZmqProducerStateTable::initialize(DBConnector *db, const std::string &tableName, bool dbPersistence)
{
    m_sendbuffer.resize(MQ_RESPONSE_MAX_COUNT);
    
    if (dbPersistence)
    {
        SWSS_LOG_DEBUG("Database persistence enabled, tableName: %s", tableName.c_str());
        m_asyncDBUpdater = std::make_unique<AsyncDBUpdater>(db, tableName);
    }
    else
    {
        SWSS_LOG_DEBUG("Database persistence disabled, tableName: %s", tableName.c_str());
        m_asyncDBUpdater = nullptr;
    }
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

    if (m_asyncDBUpdater != nullptr)
    {
        // async write need keep data till write to DB
        std::shared_ptr<KeyOpFieldsValuesTuple> clone = std::make_shared<KeyOpFieldsValuesTuple>();
        kfvKey(*clone) = key;
        kfvOp(*clone) = op;
        for(const auto &value : values)
        {
            kfvFieldsValues(*clone).push_back(value);
        }

        m_asyncDBUpdater->update(clone);
    }
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

    if (m_asyncDBUpdater != nullptr)
    {
        // async write need keep data till write to DB
        std::shared_ptr<KeyOpFieldsValuesTuple> clone = std::make_shared<KeyOpFieldsValuesTuple>();
        kfvKey(*clone) = key;
        kfvOp(*clone) = op;

        m_asyncDBUpdater->update(clone);
    }
}

void ZmqProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple> &values)
{
    for (const auto &value : values)
    {
        m_zmqClient.sendMsg(
                            kfvKey(value),
                            kfvFieldsValues(value),
                            SET_COMMAND,
                            m_dbName,
                            m_tableNameStr,
                            m_sendbuffer);
    }
    
    if (m_asyncDBUpdater != nullptr)
    {
        for (const auto &value : values)
        {
            // async write need keep data till write to DB
            std::shared_ptr<KeyOpFieldsValuesTuple> clone = std::make_shared<KeyOpFieldsValuesTuple>(value);
            m_asyncDBUpdater->update(clone);
        }
    }
}

void ZmqProducerStateTable::del(const std::vector<std::string> &keys)
{
    for (const auto &key : keys)
    {
        m_zmqClient.sendMsg(
                            key,
                            vector<FieldValueTuple>(),
                            DEL_COMMAND,
                            m_dbName,
                            m_tableNameStr,
                            m_sendbuffer);
    }
    
    if (m_asyncDBUpdater != nullptr)
    {
        for (const auto &key : keys)
        {
            // async write need keep data till write to DB
            std::shared_ptr<KeyOpFieldsValuesTuple> clone = std::make_shared<KeyOpFieldsValuesTuple>();
            kfvKey(*clone) = key;
            kfvOp(*clone) = DEL_COMMAND;
            m_asyncDBUpdater->update(clone);
        }
    }
}

}
