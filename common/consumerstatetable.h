#pragma once

#include <string>
#include <deque>
#include "dbconnector.h"
#include "table.h"
#include "redisselect.h"

namespace swss {

class ConsumerStateTable : public RedisTransactioner, public RedisSelect, public TableName_KeySet, public TableEntryPoppable
{
public:
    const int POP_BATCH_SIZE;

    ConsumerStateTable(DBConnector *db, std::string tableName);

    /* Get a singlesubscribe channel rpop */
    /* If there is nothing to pop, the output paramter will have empty key and op */
    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX);

private:
    std::deque<KeyOpFieldsValuesTuple> m_buffer;

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX);
};

}
