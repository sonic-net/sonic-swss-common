#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <thread> 
#include <mutex> 
#include "table.h"
#include "redispipeline.h"
#include "producerstatetable.h"
#include "zmqclient.h"

namespace swss {

class ZmqProducerStateTable : public ProducerStateTable
{
public:
    ZmqProducerStateTable(DBConnector *db, const std::string &tableName, ZmqClient &zmqClient);
    ZmqProducerStateTable(RedisPipeline *pipeline, const std::string &tableName, ZmqClient &zmqClient, bool buffered = false);

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

private:
    void initialize();

    ZmqClient& m_zmqClient;
    
    std::vector<char> m_sendbuffer;

    const std::string m_dbName;
    const std::string m_tableNameStr;
};

}
