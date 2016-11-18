#ifndef __CONSUMERTABLE__
#define __CONSUMERTABLE__

#include <string>
#include <vector>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"

namespace swss {

class ConsumerTable : public RedisTransactioner, public RedisSelect, public TableName_KeyValueOpQueues, public TableEntryPoppable
{
public:
    ConsumerTable(DBConnector *db, std::string tableName);

    /* Get a singlesubsribe channel rpop */
    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX);
};

}

#endif
