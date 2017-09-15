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
    /* TODO: Inherit ConsumerStateTable from ConsumerTable to merge same functions and variables */
    const int POP_BATCH_SIZE;

    ConsumerStateTable(DBConnector *db, std::string tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE);

    /* Get a singlesubscribe channel rpop */
    /* If there is nothing to pop, the output paramter will have empty key and op */
    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX);

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX);

private:
    std::deque<KeyOpFieldsValuesTuple> m_buffer;
};

}
