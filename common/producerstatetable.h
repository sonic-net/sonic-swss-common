#pragma once

#include "table.h"
#include "redispipeline.h"

namespace swss {

typedef std::function<void ()> task;

/* Flushing adaptor is to flush the adaptee immediately */
class TableEntryAsyncWritable : public TableEntryWritable
{
    
    virtual task setAsync(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND) = 0;

    virtual task delAsync(std::string key, std::string op = DEL_COMMAND) = 0;

    virtual void flush() = 0;
};

class ProducerStateTable : public TableName_KeySet, public TableEntryAsyncWritable
{
public:
    ProducerStateTable(DBConnector *db, std::string tableName);
    ~ProducerStateTable();

    /* Implements set() and del() commands using notification messages */
    virtual void set(std::string key,
                     std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND,
                     std::string prefix = EMPTY_PREFIX);

    virtual void del(std::string key,
                     std::string op = DEL_COMMAND,
                     std::string prefix = EMPTY_PREFIX);

    virtual task setAsync(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND);

    virtual task delAsync(std::string key, std::string op = DEL_COMMAND);

    virtual void flush();

private:
    DBConnector* m_db;
    RedisPipeline* m_pipe;
    std::string shaSet;
    std::string shaDel;
};

}
