#pragma once

#include "table.h"

namespace swss {

class ProducerStateTable : public TableName_KeySet, public TableEntryWritable
{
public:
    ProducerStateTable(DBConnector *db, std::string tableName);

    /* Implements set() and del() commands using notification messages */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND);

    virtual void del(std::string key, std::string op = DEL_COMMAND);
    
private:
    DBConnector* m_db;
};

}

