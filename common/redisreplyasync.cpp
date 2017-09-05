#include <string.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <system_error>
#include <functional>

#include "common/logger.h"
#include "common/redisreplyasync.h"
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
        SWSS_LOG_ERROR("RedisReplyAsync catches system_error: command: %s, reason: %s", command, ex.what());
        throw;
    }
}

RedisReplyAsync::RedisReplyAsync(DBConnector *db, const RedisCommand& command)
{
    SWSS_LOG_DEBUG("RedisReplyAsync non-blocking RedisCommand: %s", command.c_str());
    redisAppendFormattedCommand(db->getContext(), command.c_str(), command.length());
    nonBlockingRedisReply(db);
    guard([&]{checkReply();}, command.c_str());
}

RedisReplyAsync::RedisReplyAsync(DBConnector *db, string command)
{
    SWSS_LOG_DEBUG("RedisReplyAsync non-blocking command: %s", command.c_str());
    redisAppendCommand(db->getContext(), command.c_str());
    nonBlockingRedisReply(db);
    guard([&]{checkReply();}, command.c_str());
}

RedisReplyAsync::RedisReplyAsync(DBConnector *db, const RedisCommand& command, int expectedType)
    : RedisReplyAsync(db, command)
{
    guard([&]{checkReplyType(expectedType);}, command.c_str());
}

RedisReplyAsync::RedisReplyAsync(DBConnector *db, string command, int expectedType)
    : RedisReplyAsync(db, command)
{
    guard([&]{checkReplyType(expectedType);}, command.c_str());
}

/*
 * For async reply processing on non-blocking db connection.
 * It should be called when the socket is readable.
 */
RedisReplyAsync::RedisReplyAsync(DBConnector *db)
{
    int status;

    if(db->isBlocking())
    {
        SWSS_LOG_ERROR("Async Redis reply expects non-blocking DB connection");

        throw system_error(make_error_code(errc::io_error),
                           "Wrong DB connection type");
    }
    redisContext *c = db->getContext();

    status = redisBufferRead(c);
    if (status== REDIS_ERR)
    {
        SWSS_LOG_ERROR("Redis reply redisBufferRead err");

        throw system_error(make_error_code(errc::io_error),
                           "redisBufferRead error");
    }
    m_reply = NULL;
    m_db = db;
}

RedisReplyAsync::~RedisReplyAsync()
{
    if(m_reply != NULL)
        freeReplyObject(m_reply);
}

redisReply *RedisReplyAsync::getContext()
{
    if(m_reply != NULL)
    {
        freeReplyObject(m_reply);
        m_reply = NULL;
    }

    redisContext *c = m_db->getContext();
    redisGetReply(c, (void**)&m_reply);

    return m_reply;
}


void RedisReplyAsync::checkStatus(char *status)
{
    if (m_reply == NULL)
        return;

    if (strcmp(m_reply->str, status) != 0)
    {
        SWSS_LOG_ERROR("Redis reply %s != %s", m_reply->str, status);

        throw system_error(make_error_code(errc::io_error),
                           "Invalid return code");
    }
}

void RedisReplyAsync::checkStatusOK()
{
    checkStatus("OK");
}


void RedisReplyAsync::checkReply()
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

void RedisReplyAsync::checkReplyType(int expectedType)
{
    if (m_reply == NULL)
        return;

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

/* For command execution on non-blocking db connection */
void RedisReplyAsync::nonBlockingRedisReply(DBConnector *db)
{
    int wdone = 0;
    void *aux = NULL;
    redisContext *c = db->getContext();

    do {
        if (redisBufferWrite(c, &wdone) == REDIS_ERR)
            throw system_error(make_error_code(errc::io_error),
                       "redisBufferWrite error");
    } while (!wdone);

    /* Read until there is a reply */
    do {
        if (redisBufferRead(c) == REDIS_ERR)
            throw system_error(make_error_code(errc::io_error),
                       "redisBufferRead error");
        if (redisGetReplyFromReader(c, &aux) == REDIS_ERR)
            throw system_error(make_error_code(errc::io_error),
                       "redisGetReplyFromReader error");
    } while (aux == NULL);
    m_reply = (redisReply *)aux;
    m_db = db;
}


}
