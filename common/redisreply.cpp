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
        SWSS_LOG_ERROR("RedisReply catches system_error: command: %s, reason: %s", command, ex.what());
        throw;
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

redisReply *RedisReply::getChild(size_t index)
{
    if (index >= m_reply->elements)
    {
        throw out_of_range("Out of the range of redisReply elements");
    }
    return m_reply->element[index];
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

        SWSS_LOG_ERROR("Expected to get redis type %d got type %d, err: %s",
                      expectedType, m_reply->type, err);

        throw system_error(make_error_code(errc::io_error),
                           "Wrong expected type of result");
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

}
