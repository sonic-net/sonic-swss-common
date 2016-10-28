#include "redisclient.h"
#include "logger.h"
#include "rediscommand.h"

using namespace std;

namespace swss
{

RedisClient::RedisClient(swss::DBConnector *db):
    m_db(db)
{
}

int64_t RedisClient::del(string key)
{
    RedisCommand sdel;
    sdel.format("DEL %s", key.c_str());
    RedisReply r(m_db, sdel, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

int64_t RedisClient::hdel(string key, string field)
{
    RedisCommand shdel;
    shdel.format("HDEL %s %s", key.c_str(), field.c_str());
    RedisReply r(m_db, shdel, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

void RedisClient::hset(string key, string field, string value)
{
    RedisCommand shset;
    shset.format("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    RedisReply r(m_db, shset, REDIS_REPLY_INTEGER);
}

void RedisClient::set(string key, string value)
{
    RedisCommand sset;
    sset.format("SET %s %s", key.c_str(), value.c_str());
    RedisReply r(m_db, sset, REDIS_REPLY_STATUS);
}

unordered_map<string, string> RedisClient::hgetall(string key)
{
    RedisCommand sincr;
    sincr.format("HGETALL %s", key.c_str());
    RedisReply r(m_db, sincr, REDIS_REPLY_ARRAY);

    auto ctx = r.getContext();

    unordered_map<string, string> map;
    for (unsigned int i = 0; i < ctx->elements; i += 2)
        map[string(ctx->element[i]->str)] = string(ctx->element[i+1]->str);

    return map;
}

vector<string> RedisClient::keys(string key)
{
    RedisCommand skeys;
    skeys.format("KEYS %s", key.c_str());
    RedisReply r(m_db, skeys, REDIS_REPLY_ARRAY);

    auto ctx = r.getContext();

    vector<string> list;
    for (unsigned int i = 0; i < ctx->elements; i++)
        list.push_back(ctx->element[i]->str);

    return list;
}

int64_t RedisClient::incr(string key)
{
    RedisCommand sincr;
    sincr.format("INCR %s", key.c_str());
    RedisReply r(m_db, sincr, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

int64_t RedisClient::decr(string key)
{
    RedisCommand sdecr;
    sdecr.format("DECR %s", key.c_str());
    RedisReply r(m_db, sdecr, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

shared_ptr<string> RedisClient::get(string key)
{
    RedisCommand sget;
    sget.format("GET %s", key.c_str());
    RedisReply r(m_db, sget);
    auto reply = r.getContext();
    
    if (reply->type == REDIS_REPLY_NIL)
    {
        return shared_ptr<string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        shared_ptr<string> ptr(new string(reply->str));
        return ptr;
    }

    throw runtime_error("GET failed, memory exception");
}

shared_ptr<string> RedisClient::hget(string key, string field)
{
    RedisCommand shget;
    shget.format("HGET %s %s", key.c_str(), field.c_str());
    RedisReply r(m_db, shget);
    auto reply = r.getContext();

    if (reply->type == REDIS_REPLY_NIL)
    {
        return shared_ptr<string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        shared_ptr<string> ptr(new string(reply->str));
        return ptr;
    }

    SWSS_LOG_ERROR("HGET failed, reply-type: %d, %s: %s", reply->type, key.c_str(), field.c_str());
    throw runtime_error("HGET failed, unexpected reply type, memory exception");
}

int64_t RedisClient::rpush(string list, string item)
{
    RedisCommand srpush;
    srpush.format("RPUSH %s %s", list.c_str(), item.c_str());
    RedisReply r(m_db, srpush, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

shared_ptr<string> RedisClient::blpop(string list, int timeout)
{
    RedisCommand sblpop;
    sblpop.format("BLPOP %s %d", list.c_str(), timeout);
    RedisReply r(m_db, sblpop);
    auto reply = r.getContext();

    if (reply->type == REDIS_REPLY_NIL)
    {
        return shared_ptr<string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        shared_ptr<string> ptr(new string(reply->str));
        return ptr;
    }

    throw runtime_error("GET failed, memory exception");
}

}
