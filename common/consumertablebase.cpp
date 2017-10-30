#include "consumertablebase.h"

namespace swss {

ConsumerTableBase::ConsumerTableBase(DBConnector *db, std::string tableName, int popBatchSize):
        TableConsumable(tableName),
        RedisTransactioner(db),
        POP_BATCH_SIZE(popBatchSize)
{
}

ConsumerTableBase::~ConsumerTableBase()
{
}

void ConsumerTableBase::pop(KeyOpFieldsValuesTuple &kco, std::string prefix)
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

void ConsumerTableBase::pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, std::string prefix)
{
    KeyOpFieldsValuesTuple kco;

    if (m_buffer.empty())
    {
        pops(m_buffer, prefix);

        if (m_buffer.empty())
        {
            fvs.clear();
            key.clear();
            op.clear();
            return;
        }
    }

    kco = m_buffer.front();
    m_buffer.pop_front();

    key = kfvKey(kco);
    op = kfvOp(kco);
    fvs = kfvFieldsValues(kco);
}

}
