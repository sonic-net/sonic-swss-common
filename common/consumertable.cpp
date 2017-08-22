#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <system_error>
#include "common/redisreply.h"
#include "common/consumertable.h"
#include "common/json.h"
#include "common/logger.h"
#include "common/redisapi.h"

using namespace std;

namespace swss {

ConsumerTable::ConsumerTable(DBConnector *db, string tableName, int popBatchSize)
    : TableConsumable(tableName)
    , RedisTransactioner(db)
    , TableName_KeyValueOpQueues(tableName)
    , POP_BATCH_SIZE(popBatchSize)
{
    for (;;)
    {
        RedisReply watch(m_db, string("WATCH ") + getKeyQueueTableName(), REDIS_REPLY_STATUS);
        watch.checkStatusOK();
        multi();
        enqueue(string("LLEN ") + getKeyQueueTableName(), REDIS_REPLY_INTEGER);
        subscribe(m_db, getChannelName());
        enqueue(string("LLEN ") + getKeyQueueTableName(), REDIS_REPLY_INTEGER);
        bool succ = exec();
        if (succ) break;
    }

    RedisReply r(dequeueReply());
    setQueueLength(r.getReply<long long int>());
}

void ConsumerTable::pop(KeyOpFieldsValuesTuple &kco, string prefix)
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

void ConsumerTable::pops(deque<KeyOpFieldsValuesTuple> &vkco, string prefix)
{
    static std::string luaScript = loadLuaScript("consumer_table_pops.lua");

    static string sha = loadRedisScript(m_db, luaScript);
    RedisCommand command;
    command.format(
        "EVALSHA %s 4 %s %s %s %s %d '' '' ''",
        sha.c_str(),
        getKeyQueueTableName().c_str(),
        getOpQueueTableName().c_str(),
        getValueQueueTableName().c_str(),
        (prefix+getTableName()).c_str(),
        POP_BATCH_SIZE);

    RedisReply r(m_db, command, REDIS_REPLY_ARRAY);

    auto ctx0 = r.getContext();
    vkco.clear();

    // if the set is empty, return an empty kco object
    if (r.getContext()->type == REDIS_REPLY_NIL)
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
        string key = ctx->element[0]->str;
        kfvKey(kco) = key;
        string op  = ctx->element[1]->str;
        kfvOp(kco) = op;

        for (size_t i = 2; i < ctx->elements; i += 2)
        {
            if (i+1 >= ctx->elements)
            {
                SWSS_LOG_ERROR("invalid number of elements in returned table: %lu >= %lu", i+1, ctx->elements);
                throw runtime_error("invalid number of elements in returned table");
            }

            FieldValueTuple e;

            fvField(e) = ctx->element[i+0]->str;
            fvValue(e) = ctx->element[i+1]->str;
            values.push_back(e);
        }
    }
}

}
