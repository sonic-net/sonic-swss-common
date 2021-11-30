#include <string.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <system_error>
#include <functional>

#include "common/logger.h"
#include "common/redisreply.h"
#include "common/dbconnector.h"
#include "common/rediscommand.h"

using namespace std;

namespace swss {

template <typename FUNC>
inline void guard(FUNC func, const char* command)
{
    try
    {
        func();
    }
    catch (const system_error& ex)
    {
        // Combine more error message and throw again
        string errmsg = "RedisReply catches system_error: command: " + string(command) + ", reason: " + ex.what();
        SWSS_LOG_ERROR("%s", errmsg.c_str());
        throw system_error(ex.code(), errmsg.c_str());
    }
}

RedisReply::RedisReply(RedisContext *ctx, const RedisCommand& command)
{
    int rc = redisAppendFormattedCommand(ctx->getContext(), command.c_str(), command.length());
    if (rc != REDIS_OK)
    {
        // The only reason of error is REDIS_ERR_OOM (Out of memory)
        // ref: https://github.com/redis/hiredis/blob/master/hiredis.c
        throw bad_alloc();
    }

    rc = redisGetReply(ctx->getContext(), (void**)&m_reply);
    if (rc != REDIS_OK)
    {
        throw RedisError("Failed to redisGetReply with " + string(command.c_str()), ctx->getContext());
    }
    guard([&]{checkReply();}, command.c_str());
}

RedisReply::RedisReply(RedisContext *ctx, const string& command)
{
    int rc = redisAppendCommand(ctx->getContext(), command.c_str());
    if (rc != REDIS_OK)
    {
        // The only reason of error is REDIS_ERR_OOM (Out of memory)
        // ref: https://github.com/redis/hiredis/blob/master/hiredis.c
        throw bad_alloc();
    }

    rc = redisGetReply(ctx->getContext(), (void**)&m_reply);
    if (rc != REDIS_OK)
    {
        throw RedisError("Failed to redisGetReply with " + command, ctx->getContext());
    }
    guard([&]{checkReply();}, command.c_str());
}

RedisReply::RedisReply(RedisContext *ctx, const RedisCommand& command, int expectedType)
    : RedisReply(ctx, command)
{
    guard([&]{checkReplyType(expectedType);}, command.c_str());
}

RedisReply::RedisReply(RedisContext *ctx, const string& command, int expectedType)
    : RedisReply(ctx, command)
{
    guard([&]{checkReplyType(expectedType);}, command.c_str());
}

RedisReply::RedisReply(redisReply *reply) :
    m_reply(reply)
{
}

RedisReply::~RedisReply()
{
    freeReplyObject(m_reply);
}

redisReply *RedisReply::release()
{
    redisReply *ret = m_reply;
    m_reply = NULL;
    return ret;
}


redisReply *RedisReply::getContext()
{
    return m_reply;
}

size_t RedisReply::getChildCount()
{
    return m_reply->elements;
}

redisReply *RedisReply::getChild(size_t index)
{
    if (index >= m_reply->elements)
    {
        throw out_of_range("Out of the range of redisReply elements");
    }
    return m_reply->element[index];
}

redisReply *RedisReply::releaseChild(size_t index)
{
    auto ret = getChild(index);
    m_reply->element[index] = NULL;
    return ret;
}

void RedisReply::checkStatus(const char *status)
{
    if (strcmp(m_reply->str, status) != 0)
    {
        SWSS_LOG_ERROR("Redis reply %s != %s", m_reply->str, status);

        throw system_error(make_error_code(errc::io_error),
                           "Invalid return code");
    }
}

void RedisReply::checkReply()
{
    if (!m_reply)
    {
        throw system_error(make_error_code(errc::not_enough_memory),
                           "Memory exception, reply is null");
    }

    if (m_reply->type == REDIS_REPLY_ERROR)
    {
        system_error ex(make_error_code(errc::io_error),
                           m_reply->str);
        freeReplyObject(m_reply);
        m_reply = NULL;
        throw ex;
    }
}

void RedisReply::checkReplyType(int expectedType)
{
    if (m_reply->type != expectedType)
    {
        const char *err = (m_reply->type == REDIS_REPLY_STRING || m_reply->type == REDIS_REPLY_ERROR) ?
            m_reply->str : "NON-STRING-REPLY";

        string errmsg = "Expected to get redis type " + to_string(expectedType) + " got type " + to_string(m_reply->type) + ", err: " + err;
        SWSS_LOG_ERROR("%s", errmsg.c_str());
        throw system_error(make_error_code(errc::io_error), errmsg);
    }
}

void RedisReply::checkStatusOK()
{
    checkStatus("OK");
}

void RedisReply::checkStatusQueued()
{
    checkStatus("QUEUED");
}

template<> long long int RedisReply::getReply<long long int>()
{
    return getContext()->integer;
}

template<> string RedisReply::getReply<string>()
{
    char *s = getContext()->str;
    if (s == NULL) return string();
    return string(s);
}

template<> RedisMessage RedisReply::getReply<RedisMessage>()
{
    RedisMessage ret;
    /* if the Key-space notification is empty, try empty message. */
    if (getContext()->type == REDIS_REPLY_NIL)
    {
        return ret;
    }

    if (getContext()->type != REDIS_REPLY_ARRAY)
    {
        SWSS_LOG_ERROR("invalid type %d for message", getContext()->type);
        return ret;
    }
    size_t n = getContext()->elements;

    /* Expecting 4 elements for each keyspace pmessage notification */
    if (n != 4)
    {
        SWSS_LOG_ERROR("invalid number of elements %zu for message", n);
        return ret;
    }

    auto ctx = getContext()->element[0];
    ret.type = ctx->str;

    /* The second element should be the original pattern matched */
    ctx = getContext()->element[1];
    ret.pattern = ctx->str;

    ctx = getContext()->element[2];
    ret.channel = ctx->str;

    ctx = getContext()->element[3];
    ret.data = ctx->str;

    return ret;
}

}
