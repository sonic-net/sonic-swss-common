#pragma once

#include "table.h"
#include "selectable.h"

namespace swss {

class ConsumerTableBase: public TableConsumable, public RedisTransactioner
{
public:
    const int POP_BATCH_SIZE;

    ConsumerTableBase(DBConnector *db, std::string tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE);

    virtual ~ConsumerTableBase();

    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX);

    void pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, std::string prefix = EMPTY_PREFIX);
    void pop(KeyOpTuple &ko, std::vector<FieldValueTuple> &fvs, std::string prefix = EMPTY_PREFIX);

protected:

    std::deque<KeyOpFieldsValuesTuple> m_buffer;
};

}
