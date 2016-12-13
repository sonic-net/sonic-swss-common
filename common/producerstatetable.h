#pragma once

#include "table.h"
#include "redispipeline.h"

namespace swss {


class ProducerStateTable : public TableName_KeySet
{
public:
    ProducerStateTable(RedisPipeline *pipeline, std::string tableName, bool buffered = false);
    ~ProducerStateTable();

    /* Implements set() and del() commands using notification messages */
    virtual void set(std::string key,
                     std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND,
                     std::string prefix = EMPTY_PREFIX);

    virtual void del(std::string key,
                     std::string op = DEL_COMMAND,
                     std::string prefix = EMPTY_PREFIX);

    void flush();

private:
    bool m_buffered;
    RedisPipeline* m_pipe;
    std::string shaSet;
    std::string shaDel;
};

}
