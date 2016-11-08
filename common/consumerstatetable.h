#pragma once

#include <string>
#include "dbconnector.h"
#include "table.h"
#include "redisselect.h"

namespace swss {

class ConsumerStateTable : public RedisTransactioner, public RedisSelect, public TableName_KeySet, public TableEntryPoppable
{
public:
    ConsumerStateTable(DBConnector *db, std::string tableName);

    /* Get a singlesubsribe channel rpop */
    /* If there is nothing to pop, the output paramter will have empty key and op */
    void pop(KeyOpFieldsValuesTuple &kco);
};

}
