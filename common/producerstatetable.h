#pragma once

#include "table.h"
#include "redispipeline.h"

namespace swss {

class ProducerStateTable : public TableName_KeySet, public TableEntryWritable
{
public:
    ProducerStateTable(DBConnector *db, std::string tableName);
    ~ProducerStateTable();

    /* Implements set() and del() commands using notification messages */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND);

    virtual void del(std::string key, std::string op = DEL_COMMAND);

    virtual void flush();

private:
    DBConnector* m_db;
    RedisPipeline* m_pipe;
    std::string shaSet;
    std::string shaDel;
};

}
