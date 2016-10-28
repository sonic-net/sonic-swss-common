#include "redisclient.h"
#include "logger.h"
#include "rediscommand.h"

namespace swss
{

RedisClient::RedisClient(swss::DBConnector *db):
    m_db(db)
{
}

int64_t RedisClient::del(std::string key)
{
    RedisCommand sdel;
    sdel.format("DEL %s", key.c_str());
    RedisReply r(m_db, sdel, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

int64_t RedisClient::hdel(std::string key, std::string field)
{
    RedisCommand shdel;
    shdel.format("HDEL %s %s", key.c_str(), field.c_str());
    RedisReply r(m_db, shdel, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

void RedisClient::hset(std::string key, std::string field, std::string value)
{
    RedisCommand shset;
    shset.format("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    RedisReply r(m_db, shset, REDIS_REPLY_INTEGER);
}

void RedisClient::set(std::string key, std::string value)
{
    RedisCommand sset;
    sset.format("SET %s %s", key.c_str(), value.c_str());
    RedisReply r(m_db, sset, REDIS_REPLY_STATUS);
}

std::unordered_map<std::string, std::string> RedisClient::hgetall(std::string key)
{
    RedisCommand sincr;
    sincr.format("HGETALL %s", key.c_str());
    RedisReply r(m_db, sincr, REDIS_REPLY_ARRAY);

    auto ctx = r.getContext();

    std::unordered_map<std::string, std::string> map;
    for (unsigned int i = 0; i < ctx->elements; i += 2)
        map[std::string(ctx->element[i]->str)] = std::string(ctx->element[i+1]->str);

    return map;
}

std::vector<std::string> RedisClient::keys(std::string key)
{
    RedisCommand skeys;
    skeys.format("KEYS %s", key.c_str());
    RedisReply r(m_db, skeys, REDIS_REPLY_ARRAY);

    auto ctx = r.getContext();

    std::vector<std::string> list;
    for (unsigned int i = 0; i < ctx->elements; i++)
        list.push_back(ctx->element[i]->str);

    return list;
}

int64_t RedisClient::incr(std::string key)
{
    RedisCommand sincr;
    sincr.format("INCR %s", key.c_str());
    RedisReply r(m_db, sincr, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

int64_t RedisClient::decr(std::string key)
{
    RedisCommand sdecr;
    sdecr.format("DECR %s", key.c_str());
    RedisReply r(m_db, sdecr, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

std::shared_ptr<std::string> RedisClient::get(std::string key)
{
    RedisCommand sget;
    sget.format("GET %s", key.c_str());
    RedisReply r(m_db, sget);
    auto reply = r.getContext();
    
    if (reply->type == REDIS_REPLY_NIL)
    {
        return std::shared_ptr<std::string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        std::shared_ptr<std::string> ptr(new std::string(reply->str));
        return ptr;
    }

    throw std::runtime_error("GET failed, memory exception");
}

std::shared_ptr<std::string> RedisClient::hget(std::string key, std::string field)
{
    RedisCommand shget;
    shget.format("HGET %s %s", key.c_str(), field.c_str());
    RedisReply r(m_db, shget);
    auto reply = r.getContext();

    if (reply->type == REDIS_REPLY_NIL)
    {
        return std::shared_ptr<std::string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        std::shared_ptr<std::string> ptr(new std::string(reply->str));
        return ptr;
    }

    SWSS_LOG_ERROR("HGET failed, reply-type: %d, %s: %s", reply->type, key.c_str(), field.c_str());
    throw std::runtime_error("HGET failed, unexpected reply type, memory exception");
}

int64_t RedisClient::rpush(std::string list, std::string item)
{
    RedisCommand srpush;
    srpush.format("RPUSH %s %s", list.c_str(), item.c_str());
    RedisReply r(m_db, srpush, REDIS_REPLY_INTEGER);
    return r.getContext()->integer;
}

std::shared_ptr<std::string> RedisClient::blpop(std::string list, int timeout)
{
    RedisCommand sblpop;
    sblpop.format("BLPOP %s %d", list.c_str(), timeout);
    RedisReply r(m_db, sblpop);
    auto reply = r.getContext();

    if (reply->type == REDIS_REPLY_NIL)
    {
        return std::shared_ptr<std::string>(NULL);
    }

    if (reply->type == REDIS_REPLY_STRING)
    {
        std::shared_ptr<std::string> ptr(new std::string(reply->str));
        return ptr;
    }

    throw std::runtime_error("GET failed, memory exception");
}

}
