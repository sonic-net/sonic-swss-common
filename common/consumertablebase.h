#pragma once

#include <deque>
#include "table.h"
#include "selectable.h"

namespace swss {

class ConsumerTableBase: public TableConsumable, public RedisTransactioner
{
public:
    const int POP_BATCH_SIZE;

    ConsumerTableBase(DBConnector *db, const std::string &tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);

    ~ConsumerTableBase() override = default;

    const DBConnector* getDbConnector() const;

    void pop(KeyOpFieldsValuesTuple &kco, const std::string &prefix = EMPTY_PREFIX);

    void pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, const std::string &prefix = EMPTY_PREFIX);

    bool empty() const { return m_buffer.empty(); };

    bool hasData() override;

    bool hasCachedData() override;

    void updateAfterRead() override;

protected:

    std::deque<KeyOpFieldsValuesTuple> m_buffer;
};

}
