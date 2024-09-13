#pragma once

#include <string>
#include <deque>
#include "dbconnector.h"
#include "consumertablebase.h"

namespace swss {

class ConsumerStateTable : public ConsumerTableBase, public TableName_KeySet
{
public:
    ConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX);

private:
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, int popBatchSize, const std::string &prefix = EMPTY_PREFIX);
    std::string m_shaPop;
};

}
