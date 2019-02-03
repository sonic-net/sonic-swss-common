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

ConsumerStateTable::ConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize, int pri)
    : ConsumerTableBase(db, tableName, popBatchSize, pri)
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

    RedisReply r(dequeueReply());
    setQueueLength(r.getReply<long long int>());

    std::string luaMultiPublish =
        "for i = 1, KEYS[2] do\n"
        "    redis.call('PUBLISH', KEYS[1], ARGV[1])\n"
        "end\n";
    m_multiPublish = loadRedisScript(m_db, luaMultiPublish);

   std::string luaScardPublish =
        "local num = redis.call('SCARD', KEYS[2])\n"
        "if num > 0 then\n"
        "    redis.call('PUBLISH', KEYS[1], ARGV[1])\n"
        "end\n";
    m_scardPublish = loadRedisScript(m_db, luaScardPublish);
}

void ConsumerStateTable::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string& /*prefix*/)
{
    static std::string luaScript = loadLuaScript("consumer_state_table_pops.lua");

    static std::string sha = loadRedisScript(m_db, luaScript);

    RedisCommand command;
    command.format(
        "EVALSHA %s 3 %s %s: %s %d %s",
        sha.c_str(),
        getKeySetName().c_str(),
        getTableName().c_str(),
        getDelKeySetName().c_str(),
        POP_BATCH_SIZE,
        getStateHashPrefix().c_str());

    RedisReply r(m_db, command);
    auto ctx0 = r.getContext();
    vkco.clear();

    // Check whether the keyset is empty, if not, signal ourselves to process again.
    // This is to handle the case where the number of keys in keyset is more than POP_BATCH_SIZE
    // Note there is possibility of false positive since this call is not atomic with consumer_state_table_pops.lua
    // Putting publish call inside consumer_state_table_pops.lua doesn't seem to work.
    command.format(
        "EVALSHA %s 2 %s %s %s",
        m_scardPublish.c_str(),
        getChannelName().c_str(),
        getKeySetName().c_str(),
        "G");
    RedisReply r2(m_db, command);

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

void ConsumerStateTable::pop(KeyOpFieldsValuesTuple &kco, const std::string &prefix)
{
    pop(kfvKey(kco), kfvOp(kco), kfvFieldsValues(kco), prefix);
}

void ConsumerStateTable::pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, const std::string &prefix)
{
    bool emptyBuff = m_buffer.empty();
    ConsumerTableBase::pop(key, op, fvs, prefix);

    // This is to generate notification to user of pop call, since user expect one notification per key.
    if (emptyBuff && !m_buffer.empty())
    {
        RedisCommand command;
        command.format(
            "EVALSHA %s 2 %s %d %s ",
            m_multiPublish.c_str(),
            getChannelName().c_str(),
            m_buffer.size(),
            "G");
        RedisReply r(m_db, command);
    }
}

}
