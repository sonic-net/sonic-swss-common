#include <string>
#include <vector>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"
#include "redisapi.h"
#include "consumerstatetable.h"

namespace swss {

ConsumerStateTable::ConsumerStateTable(DBConnector *db, std::string tableName)
    : RedisTransactioner(db)
    , TableName_KeySet(tableName)
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

    long long int len = queueResultsFront()->integer;
    setQueueLength(len);
}

void ConsumerStateTable::pop(KeyOpFieldsValuesTuple &kco)
{
    auto& values = kfvFieldsValues(kco);
    values.clear();

    static std::string luaScript =
        "local key = redis.call('SPOP', KEYS[1])\n"
        "if not key then return end\n"
        "local values = redis.call('HGETALL', KEYS[2] .. key)\n"
        "local ret = {key, values}\n"
        "return ret\n";

    static std::string sha = loadRedisScript(m_db, luaScript);

    RedisCommand command;
    command.format(
        "EVALSHA %s 2 %s %s: '' ''",
        sha.c_str(),
        getKeySetName().c_str(),
        getTableName().c_str());

    RedisReply r(m_db, command);
    auto ctx = r.getContext();

    // if the set is empty, return an empty kco object
    if (r.getContext()->type == REDIS_REPLY_NIL)
    {
        kfvKey(kco).clear();
        kfvOp(kco).clear();
        return;
    }

    assert(ctx->type == REDIS_REPLY_ARRAY);
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

    // There is one limitation of lua script in redis
    // Write commands not allowed after non deterministic commands
    // So we cannot conditionally increase counter in the same lua script
    RedisCommand inc;
    inc.format("HINCRBY %s pop 1", getCounterName().c_str());
    RedisReply rinc(m_db, inc, REDIS_REPLY_INTEGER);
}

}
