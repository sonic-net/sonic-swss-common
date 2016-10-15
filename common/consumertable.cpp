#include "common/redisreply.h"
#include "common/consumertable.h"
#include "common/json.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <system_error>

/* The database is already alive and kicking, no need for more than a second */
#define SUBSCRIBE_TIMEOUT (1000)

using namespace std;

namespace swss {

ConsumerTable::ConsumerTable(DBConnector *db, string tableName) :
    Table(db, tableName),
    m_subscribe(NULL),
    m_queueLength(0)
{
    bool again = true;

    while (again)
    {
        try
        {
            RedisReply watch(m_db, string("WATCH ") + getKeyQueueTableName(), REDIS_REPLY_STATUS);
            watch.checkStatusOK();
            multi();
            enqueue(string("LLEN ") + getKeyQueueTableName(), REDIS_REPLY_INTEGER);
            subsribe();
            enqueue(string("LLEN ") + getKeyQueueTableName(), REDIS_REPLY_INTEGER);
            exec();
            again = false;
        }
        catch (...)
        {
            delete m_subscribe;
            m_subscribe = NULL;
        }
    }

    m_queueLength = queueResultsFront()->integer;
    /* No need for that since we have WATCH gurantee on the transaction */
}

ConsumerTable::~ConsumerTable()
{
    delete m_subscribe;
}

void ConsumerTable::pop(KeyOpFieldsValuesTuple &kco)
{
    static std::string luaScript =

        "local key   = redis.call('RPOP', KEYS[1])\n"
        "local op    = redis.call('RPOP', KEYS[2])\n"
        "local value = redis.call('RPOP', KEYS[3])\n"

        "local dbop = op:sub(1,1)\n"
        "op = op:sub(2)\n"
        "local ret = {key, op}\n"

        "local hex = '0123456789ABCDEF'\n"

        "local descape = function (a)\n"
        "    local i = 0\n local s = ''\n local len = string.len(a)\n"
        "    while i < len do \n"
        "    local c = a:sub(i,i)\n"
        "    if c == '%' then\n"
        "        local h = string.find(hex, a:sub(i + 1, i + 1)) - 1\n"
        "        local l = string.find(hex, a:sub(i + 2, i + 2)) - 1\n"
        "        i = i + 2\n"
        "        s = s .. string.char((h * 16) + l)\n"
        "    else\n"
        "        s = s .. c\n"
        "    end\n"
        "    i = i + 1\n"
        "    end\n"
        "    return s\n"
        "end\n"

        "local i = 0\n local j = 0\n"
        "local json = value\n"

        "while true do\n"
        "    i = string.find(json, '\"', i + 1)\n"
        "    if i == nil then break end\n"
        "    j = string.find(json, '\"', i + 1)\n"
        "    local field = descape(string.sub(json, i + 1 , j))\n"
        "    i = string.find(json, '\"', j + 1)\n"
        "    j = string.find(json, '\"', i + 1)\n"
        "    local value = descape(string.sub(json, i + 1, j))\n"
        "    i = j\n"
        "    table.insert(ret, field)\n"
        "    table.insert(ret, value)\n"
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

    static std::string sha = scriptLoad(luaScript);

    char *temp;

    int len = redisFormatCommand(
            &temp,
            "EVALSHA %s 4 %s %s %s %s '' '' '' ''",
            sha.c_str(),
            getKeyQueueTableName().c_str(),
            getOpQueueTableName().c_str(),
            getValueQueueTableName().c_str(),
            getTableName().c_str());

    string command = string(temp, len);
    free(temp);

    RedisReply r(m_db, command, REDIS_REPLY_ARRAY, true);

    auto ctx = r.getContext();

    std::string key = ctx->element[0]->str;
    std::string op  = ctx->element[1]->str;

    vector<FieldValueTuple> fieldsValues;

    for (size_t i = 2; i < ctx->elements; i += 2)
    {
        FieldValueTuple e;

        fvField(e) = ctx->element[i+0]->str;
        fvValue(e) = ctx->element[i+1]->str;
        fieldsValues.push_back(e);
    }

    kco = std::make_tuple(key, op, fieldsValues);
}

void ConsumerTable::addFd(fd_set *fd)
{
    FD_SET(m_subscribe->getContext()->fd, fd);
}

int ConsumerTable::readCache()
{
    redisReply *reply = NULL;

    /* Read the messages in queue before subsribe command execute */
    if (m_queueLength) {
        m_queueLength--;
        return ConsumerTable::DATA;
    }

    if (redisGetReplyFromReader(m_subscribe->getContext(),
                                (void**)&reply) != REDIS_OK)
    {
        return Selectable::ERROR;
    } else if (reply != NULL)
    {
        freeReplyObject(reply);
        return Selectable::DATA;
    }

    return Selectable::NODATA;
}

void ConsumerTable::readMe()
{
    redisReply *reply = NULL;

    if (redisGetReply(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
        throw "Unable to read redis reply";

    freeReplyObject(reply);
}

bool ConsumerTable::isMe(fd_set *fd)
{
    return FD_ISSET(m_subscribe->getContext()->fd, fd);
}

void ConsumerTable::subsribe()
{
    /* Create new new context to DB */
    if (m_db->getContext()->connection_type == REDIS_CONN_TCP)
        m_subscribe = new DBConnector(m_db->getDB(),
                                      m_db->getContext()->tcp.host,
                                      m_db->getContext()->tcp.port,
                                      SUBSCRIBE_TIMEOUT);
    else
        m_subscribe = new DBConnector(m_db->getDB(),
                                      m_db->getContext()->unix_sock.path,
                                      SUBSCRIBE_TIMEOUT);
    /* Send SUBSCRIBE #channel command */
    string s("SUBSCRIBE ");
    s+= getChannelTableName();
    RedisReply r(m_subscribe, s, REDIS_REPLY_ARRAY);
}

}
