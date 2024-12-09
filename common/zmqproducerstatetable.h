#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <thread> 
#include <mutex> 
#include "asyncdbupdater.h"
#include "producerstatetable.h"
#include "redispipeline.h"
#include "table.h"
#include "zmqclient.h"

namespace swss {

class ZmqProducerStateTable : public ProducerStateTable
{
public:
    ZmqProducerStateTable(DBConnector *db, const std::string &tableName, ZmqClient &zmqClient, bool dbPersistence = true);
    ZmqProducerStateTable(RedisPipeline *pipeline, const std::string &tableName, ZmqClient &zmqClient, bool buffered = false, bool dbPersistence = true);

    /* Implements set() and del() commands using notification messages */
    virtual void set(const std::string &key,
                     const std::vector<FieldValueTuple> &values,
                     const std::string &op = SET_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX);

    virtual void del(const std::string &key,
                     const std::string &op = DEL_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX);

    // Batched version of set() and del().
    virtual void set(const std::vector<KeyOpFieldsValuesTuple> &values);

    virtual void del(const std::vector<std::string> &keys);

    // Batched send that can include both SET and DEL requests.
    virtual void send(const std::vector<KeyOpFieldsValuesTuple> &kcos);

    size_t dbUpdaterQueueSize();
private:
    void initialize(DBConnector *db, const std::string &tableName, bool dbPersistence);

    ZmqClient& m_zmqClient;

    const std::string m_dbName;
    const std::string m_tableNameStr;

    std::unique_ptr<AsyncDBUpdater> m_asyncDBUpdater;
};

}
