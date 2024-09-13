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

// Redis will not response to other process when run shapop scripts with very big batch size.
static const size_t SHA_POP_COMMAN_MAX_POP_SIZE = 1024;

ConsumerStateTable::ConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize, int pri)
    : ConsumerTableBase(db, tableName, popBatchSize, pri)
    , TableName_KeySet(tableName)
{
    std::string luaScript = loadLuaScript("consumer_state_table_pops.lua");
    m_shaPop = loadRedisScript(db, luaScript);

    for (;;)
    {
        RedisReply watch(m_db, "WATCH " + getKeySetName(), REDIS_REPLY_STATUS);
        watch.checkStatusOK();
        multi();
        enqueue(std::string("SCARD ") + getKeySetName(), REDIS_REPLY_INTEGER);
        subscribe(m_db, getChannelName(m_db->getDbId()));
        bool succ = exec();
        if (succ) break;
    }

    RedisReply r(dequeueReply());
    setQueueLength(r.getReply<long long int>());
}

void ConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string& /*prefix*/)
{
    // With shaPop sciipt, the poped count may smaller than batch size
    // Use not_poped_count to control run shaPop script how many times
    // Use poped_count to calculate and resize vkco queue after pop finished
    size_t not_poped_count = POP_BATCH_SIZE;
    size_t poped_count = 0;
    vkco.clear();
    vkco.resize(not_poped_count);
    for(;;)
    {
        if (not_poped_count <= SHA_POP_COMMAN_MAX_POP_SIZE)
        {
            poped_count += popsWithBatchSize(vkco, not_poped_count, poped_count);

            // A call to resize with a smaller size does not invalidate any references to non-erased elements.
            // https://en.cppreference.com/w/cpp/container/deque
            vkco.resize(poped_count);
            return;
        }
        else
        {
            poped_count += popsWithBatchSize(vkco, SHA_POP_COMMAN_MAX_POP_SIZE, poped_count);
            not_poped_count -= SHA_POP_COMMAN_MAX_POP_SIZE;
        }
    }
}

size_t ConsumerStateTable::popsWithBatchSize(std::deque<KeyOpFieldsValuesTuple> &vkco, size_t popBatchSize, size_t queueStartIndex)
{

    RedisCommand command;
    command.format(
        "EVALSHA %s 3 %s %s%s %s %d %s",
        m_shaPop.c_str(),
        getKeySetName().c_str(),
        getTableName().c_str(),
        getTableNameSeparator().c_str(),
        getDelKeySetName().c_str(),
        (int)popBatchSize,
        getStateHashPrefix().c_str());

    RedisReply r(m_db, command);
    auto ctx0 = r.getContext();

    // if the set is empty, return an empty kco object
    if (ctx0->type == REDIS_REPLY_NIL)
    {
        return 0;
    }

    assert(ctx0->type == REDIS_REPLY_ARRAY);
    size_t n = ctx0->elements;
    for (size_t ie = 0; ie < n; ie++)
    {
        auto& kco = vkco[queueStartIndex + ie];
        auto& values = kfvFieldsValues(kco);
        assert(values.empty());

        auto& ctx = ctx0->element[ie];
        assert(ctx->element[0]->type == REDIS_REPLY_STRING);
        std::string key(ctx->element[0]->str, ctx->element[0]->len);
        kfvKey(kco) = key;

        assert(ctx->element[1]->type == REDIS_REPLY_ARRAY);
        auto ctx1 = ctx->element[1];
        for (size_t i = 0; i < ctx1->elements / 2; i++)
        {
            FieldValueTuple e;
            fvField(e).assign(ctx1->element[i * 2]->str, ctx1->element[i * 2]->len);
            fvValue(e).assign(ctx1->element[i * 2 + 1]->str, ctx1->element[i * 2 + 1]->len);
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

    return n;
}

}
