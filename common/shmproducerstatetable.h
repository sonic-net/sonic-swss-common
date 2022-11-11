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
    
enum ShmOperationType
{
    SET,
    DEL,
    BATCH_SET,
    BATCH_DEL
};

struct ShmOperationItem
{
    ShmOperationType m_type;
    
    ShmOperationItem(ShmOperationType type) 
        : m_type(type)
    {}

    virtual ~ShmOperationItem(){};
};

struct ShmOperationSet : public ShmOperationItem
{
    KeyOpFieldsValuesTuple m_values;
    
    ShmOperationSet(const std::string &key, const std::vector<FieldValueTuple> &values) 
        : ShmOperationItem(ShmOperationType::SET)
    {
        kfvKey(m_values) = key;
        for (const auto &value : values)
        {
            kfvFieldsValues(m_values).push_back(value);
        }
    }
};

struct ShmOperationDel : public ShmOperationItem
{
    std::string m_key;
    
    ShmOperationDel(const std::string &key) 
        : ShmOperationItem(ShmOperationType::DEL)
        , m_key(key) 
    {}
};

struct ShmOperationBatchSet : public ShmOperationItem
{
    std::vector<KeyOpFieldsValuesTuple> m_values;
    
    ShmOperationBatchSet(const std::vector<KeyOpFieldsValuesTuple>& values) 
        : ShmOperationItem(ShmOperationType::BATCH_SET)
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

struct ShmOperationBatchDel : public ShmOperationItem
{
    std::vector<std::string> m_keys;
    
    ShmOperationBatchDel(const std::vector<std::string>& keys) 
        : ShmOperationItem(ShmOperationType::BATCH_DEL)
    {
        for (const auto &key : keys)
        {
            m_keys.push_back(key);
        }
    }
};

class ShmProducerStateTable : public ProducerStateTable
{
public:
    ShmProducerStateTable(DBConnector *db, const std::string &tableName);
    ShmProducerStateTable(RedisPipeline *pipeline, const std::string &tableName, bool buffered = false);
    ~ShmProducerStateTable();

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
    void initializeThread();

    void updateTableThreadFunction();

    std::shared_ptr<std::thread> m_updateTableThread;

    bool m_runUpdateTableThread;
    
    std::queue<std::shared_ptr<ShmOperationItem>> m_operationQueue;

    std::mutex m_operationQueueMutex;
};

}
