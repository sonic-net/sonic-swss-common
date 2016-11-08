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

    setQueueLength(queueResultsFront()->integer);
}

void ConsumerTable::pop(KeyOpFieldsValuesTuple &kco)
{
    static string luaScript =

        "local key   = redis.call('RPOP', KEYS[1])\n"
        "local op    = redis.call('RPOP', KEYS[2])\n"
        "local value = redis.call('RPOP', KEYS[3])\n"

        "local dbop = op:sub(1,1)\n"
        "op = op:sub(2)\n"
        "local ret = {key, op}\n"

        "local jj = cjson.decode(value)\n"
        "local size = #jj\n"

        "for idx=1,size,2 do\n"
        "    table.insert(ret, jj[idx])\n"
        "    table.insert(ret, jj[idx+1])\n"
        "end\n"

        "if op == 'get' or op == 'getresponse' then\n"
        "return ret\n"
        "end\n"

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

        "return ret";

    static string sha = loadRedisScript(m_db, luaScript);
    RedisCommand command;
    command.format(
        "EVALSHA %s 4 %s %s %s %s '' '' '' ''",
        sha.c_str(),
        getKeyQueueTableName().c_str(),
        getOpQueueTableName().c_str(),
        getValueQueueTableName().c_str(),
        getTableName().c_str());

    RedisReply r(m_db, command, REDIS_REPLY_ARRAY);

    auto ctx = r.getContext();

    string key = ctx->element[0]->str;
    string op  = ctx->element[1]->str;

    vector<FieldValueTuple> fieldsValues;

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
        fieldsValues.push_back(e);
    }

    kco = make_tuple(key, op, fieldsValues);
}

}
