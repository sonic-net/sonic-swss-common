#pragma once

#include "table.h"
#include "selectable.h"

namespace swss {

class ConsumerTableBase: public TableConsumable, public RedisTransactioner
{
public:
    const int POP_BATCH_SIZE;

    ConsumerTableBase(DBConnector *db, std::string tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE):
        TableConsumable(tableName),
        RedisTransactioner(db),
        POP_BATCH_SIZE(popBatchSize)
    {
    }

    virtual ~ConsumerTableBase() {}

    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX)
    {
        if (m_buffer.empty())
        {
            pops(m_buffer, prefix);

            if (m_buffer.empty())
            {
                auto& values = kfvFieldsValues(kco);
                values.clear();
                kfvKey(kco).clear();
                kfvOp(kco).clear();
                return;
            }
        }

        kco = m_buffer.front();
        m_buffer.pop_front();
    }

protected:

    std::deque<KeyOpFieldsValuesTuple> m_buffer;
};

}
