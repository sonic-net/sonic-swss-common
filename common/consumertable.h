#ifndef __CONSUMERTABLE__
#define __CONSUMERTABLE__

#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"

namespace swss {

class ConsumerTable : public TableConsumable, public RedisTransactioner, public TableName_KeyValueOpQueues
{
public:
    const int POP_BATCH_SIZE;

    ConsumerTable(DBConnector *db, std::string tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE);

    /* Get a singlesubscribe channel rpop */
    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX);

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX);
private:
    std::deque<KeyOpFieldsValuesTuple> m_buffer;
};

}

#endif
