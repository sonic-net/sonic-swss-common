#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <thread> 
#include <mutex> 
#include "table.h"
#include "redispipeline.h"
#include "producerstatetable.h"

namespace swss {

class ZmqProducerStateTable : public ProducerStateTable
{
public:
    ZmqProducerStateTable(DBConnector *db, const std::string &tableName, const std::string& endpoint);
    ZmqProducerStateTable(RedisPipeline *pipeline, const std::string &tableName, const std::string& endpoint, bool buffered = false);
    ~ZmqProducerStateTable();

    /* Implements set() and del() commands using notification messages */
    virtual void set(const std::string &key,
                     const std::vector<FieldValueTuple> &values,
                     const std::string &op = SET_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX);

    virtual void del(const std::string &key,
                     const std::string &op = DEL_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX);

    // Batched version of set() and del().
    virtual void set(const std::vector<KeyOpFieldsValuesTuple>& values);

    virtual void del(const std::vector<std::string>& keys);
private:
    void initialize(const std::string& endpoint);

    void connect(const std::string& endpoint);

    void sendMsg(const std::string& key,
                 const std::vector<swss::FieldValueTuple>& values,
                 const std::string& command);

    std::string m_endpoint;

    void* m_context;

    void* m_socket;

    bool m_connected;
    
    std::vector<char> m_sendbuffer;
};

}
