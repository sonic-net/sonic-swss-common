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

    void pop(KeyOpFieldsValuesTuple &kco, const std::string &prefix = EMPTY_PREFIX) override;
    void pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, const std::string &prefix = EMPTY_PREFIX) override;

private:
    std::string m_multiPublish;
};

}
