#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"
#include "redisapi.h"
#include "consumerstatetable.h"

namespace swss {

ConsumerStateTable::ConsumerStateTable(DBConnector *db, std::string tableName, int popBatchSize)
    : TableConsumable(tableName)
    , RedisTransactioner(db)
    , TableName_KeySet(tableName)
    , POP_BATCH_SIZE(popBatchSize)
{
    for (;;)
    {
        RedisReply watch(m_db, "WATCH " + getKeySetName(), REDIS_REPLY_STATUS);
        watch.checkStatusOK();
        multi();
        enqueue(std::string("SCARD ") + getKeySetName(), REDIS_REPLY_INTEGER);
        subscribe(m_db, getChannelName());
        bool succ = exec();
        if (succ) break;
    }

    RedisReply r(dequeueReply());
    setQueueLength(r.getReply<long long int>());
}

void ConsumerStateTable::pop(KeyOpFieldsValuesTuple &kco, std::string prefix)
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

void ConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string /*prefix*/)
{
    static std::string luaScript = loadLuaScript("consumer_state_table_pops.lua");

    static std::string sha = loadRedisScript(m_db, luaScript);

    RedisCommand command;
    command.format(
        "EVALSHA %s 2 %s %s: %d ''",
        sha.c_str(),
        getKeySetName().c_str(),
        getTableName().c_str(),
        POP_BATCH_SIZE);

    RedisReply r(m_db, command);
    auto ctx0 = r.getContext();
    vkco.clear();

    // if the set is empty, return an empty kco object
    if (ctx0->type == REDIS_REPLY_NIL)
    {
        return;
    }

    assert(ctx0->type == REDIS_REPLY_ARRAY);
    size_t n = ctx0->elements;
    vkco.resize(n);
    for (size_t ie = 0; ie < n; ie++)
    {
        auto& kco = vkco[ie];
        auto& values = kfvFieldsValues(kco);
        assert(values.empty());

        auto& ctx = ctx0->element[ie];
        assert(ctx->elements == 2);
        assert(ctx->element[0]->type == REDIS_REPLY_STRING);
        std::string key = ctx->element[0]->str;
        kfvKey(kco) = key;

        assert(ctx->element[1]->type == REDIS_REPLY_ARRAY);
        auto ctx1 = ctx->element[1];
        for (size_t i = 0; i < ctx1->elements / 2; i++)
        {
            FieldValueTuple e;
            fvField(e) = ctx1->element[i * 2]->str;
            fvValue(e) = ctx1->element[i * 2 + 1]->str;
            values.push_back(e);
        }

        // if there is no field-value pair, the key is already deleted
        if (values.empty())
        {
            kfvOp(kco) = DEL_COMMAND;
        }
        else
        {
            kfvOp(kco) = SET_COMMAND;
        }
    }
}

}
