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

ConsumerTable::ConsumerTable(DBConnector *db, string tableName)
    : RedisTransactioner(db)
    , TableName_KeyValueOpQueues(tableName)
    , POP_BATCH_SIZE(128)
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
    }

    if (m_buffer.empty())
    {
        auto& values = kfvFieldsValues(kco);
        values.clear();
        kfvKey(kco).clear();
        kfvOp(kco).clear();
        return;
    }

    kco = m_buffer.front();
    m_buffer.pop_front();
}

void ConsumerTable::pops(deque<KeyOpFieldsValuesTuple> &vkco, string prefix)
{
    static string luaScript =
        "local rets = {}\n"
        "local keys   = redis.call('LRANGE', KEYS[1], -ARGV[1], -1)\n"
        "local ops    = redis.call('LRANGE', KEYS[2], -ARGV[1], -1)\n"
        "local values = redis.call('LRANGE', KEYS[3], -ARGV[1], -1)\n"
        "redis.call('LTRIM', KEYS[1], 0, -ARGV[1]-1)"
        "redis.call('LTRIM', KEYS[2], 0, -ARGV[1]-1)"
        "redis.call('LTRIM', KEYS[3], 0, -ARGV[1]-1)"

        "local n = table.getn(keys)\n"
        "for i = n, 1, -1 do\n"
            "local key = keys[i]\n"
            "local op = ops[i]\n"
            "local value = values[i]\n"
            "local dbop = op:sub(1,1)\n"
            "op = op:sub(2)\n"
            "local ret = {key, op}\n"

            "local jj = cjson.decode(value)\n"
            "local size = #jj\n"

            "for idx=1,size,2 do\n"
            "    table.insert(ret, jj[idx])\n"
            "    table.insert(ret, jj[idx+1])\n"
            "end\n"
            "table.insert(rets, ret)\n"

            "if op == 'bulkset' or op == 'bulkcreate' then\n"

            // key is "OBJECT_TYPE:num", extract object type from key
            "    key = key:sub(1, string.find(key, ':') - 1)\n"

            "   local len = #ret\n"
            "   local st = 3\n"         // since 1 and 2 is key/op
            "   while st <= len do\n"
            "       local field = ret[st]\n"
            // keyname is ASIC_STATE : OBJECT_TYPE : OBJECT_ID
            "       local keyname = KEYS[4] .. ':' .. key .. ':' .. field\n"

            // value can be multiple a=v|a=v|... we need to split using gmatch
            "       local vars = ret[st+1]\n"
            "       for value in string.gmatch(vars,'([^|]+)') do\n"
            "           local attr = value:sub(1, string.find(value, '=') - 1)\n"
            "           local val = value.sub(value, string.find(value, '=') + 1)\n"
            "           redis.call('HSET', keyname, attr, val)\n"
            "       end\n"

            "       st = st + 2\n"
            "   end\n"

            "elseif op ~= 'get' and op ~= 'getresponse' and op ~= 'notify' then\n"
                "local keyname = KEYS[4] .. ':' .. key\n"
                "if key == '' then\n"
                "   keyname = KEYS[4]\n"
                "end\n"

                "if dbop == 'D' then\n"
                "   redis.call('DEL', keyname)\n"
                "else\n"
                "   local st = 3\n"
                "   local len = #ret\n"
                "   while st <= len do\n"
                "       redis.call('HSET', keyname, ret[st], ret[st+1])\n"
                "       st = st + 2\n"
                "   end\n"
                "end\n"
            "end\n"
        "end\n"
        "return rets";

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
