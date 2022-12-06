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
    
enum ConfigOperationType
{
    SET,
    DEL,
    BATCH_SET,
    BATCH_DEL
};

struct ConfigOperationItem
{
    ConfigOperationType m_type;
    
    ConfigOperationItem(ConfigOperationType type) 
        : m_type(type)
    {}

    virtual ~ConfigOperationItem(){};
};

struct ConfigOperationSet : public ConfigOperationItem
{
    KeyOpFieldsValuesTuple m_values;
    
    ConfigOperationSet(const std::string &key, const std::vector<FieldValueTuple> &values) 
        : ConfigOperationItem(ConfigOperationType::SET)
    {
        kfvKey(m_values) = key;
        for (const auto &value : values)
        {
            kfvFieldsValues(m_values).push_back(value);
        }
    }
};

struct ConfigOperationDel : public ConfigOperationItem
{
    std::string m_key;
    
    ConfigOperationDel(const std::string &key) 
        : ConfigOperationItem(ConfigOperationType::DEL)
        , m_key(key) 
    {}
};

struct ConfigOperationBatchSet : public ConfigOperationItem
{
    std::vector<KeyOpFieldsValuesTuple> m_values;
    
    ConfigOperationBatchSet(const std::vector<KeyOpFieldsValuesTuple>& values) 
        : ConfigOperationItem(ConfigOperationType::BATCH_SET)
    {
        
        for (const auto &value : values)
        {
            KeyOpFieldsValuesTuple tmp;

            kfvKey(tmp) = kfvKey(value);
            for (const auto &iv : kfvFieldsValues(value))
            {
                kfvFieldsValues(tmp).push_back(iv);
            }

            m_values.push_back(tmp);
        }
    }
};

struct ConfigOperationBatchDel : public ConfigOperationItem
{
    std::vector<std::string> m_keys;
    
    ConfigOperationBatchDel(const std::vector<std::string>& keys) 
        : ConfigOperationItem(ConfigOperationType::BATCH_DEL)
    {
        for (const auto &key : keys)
        {
            m_keys.push_back(key);
        }
    }
};

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

    void updateTableThreadFunction();

    void sendMsg(const std::string& key,
                 const std::vector<swss::FieldValueTuple>& values,
                 const std::string& command);

    std::shared_ptr<std::thread> m_updateTableThread;

    bool m_runUpdateTableThread;
    
    std::queue<std::shared_ptr<ConfigOperationItem>> m_operationQueue;

    std::mutex m_operationQueueMutex;

    std::string m_endpoint;

    void* m_context;

    void* m_socket;

    bool m_connected;
};

}
