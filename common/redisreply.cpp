#include <string.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <system_error>

#include "common/logger.h"
#include "common/redisreply.h"

using namespace std;

namespace swss {

RedisReply::RedisReply(DBConnector *db, string command, int expectedType, bool isFormatted)
    : RedisReply(db, command, isFormatted)
{
    if (m_reply->type != expectedType)
    {
        printf("command=%s, type=%d, etype=%d\n", command.c_str(), m_reply->type, expectedType);
        const char *err = (m_reply->type == REDIS_REPLY_STRING || m_reply->type == REDIS_REPLY_ERROR) ?
            m_reply->str : "NON-STRING-REPLY";

        SWSS_LOG_ERROR("Expected to get redis type %d got type %d, command: %s, err: %s",
                      expectedType, m_reply->type, command.c_str(), err);
        freeReplyObject(m_reply);

        throw system_error(make_error_code(errc::io_error),
                           "Wrong expected type of result");
    }
}

RedisReply::RedisReply(DBConnector *db, std::string command, bool isFormatted)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("Redis reply command: %s", command.c_str());

    if (isFormatted)
    {
        redisAppendFormattedCommand(db->getContext(), command.c_str(), command.length());
        redisGetReply(db->getContext(), (void**)&m_reply);
    }
    else
    {
        m_reply = (redisReply *)redisCommand(db->getContext(), command.c_str());
    }

    if (!m_reply)
    {
        SWSS_LOG_ERROR("Redis reply is NULL, memory exception, command: %s", command.c_str());

        throw system_error(make_error_code(errc::not_enough_memory),
                           "Memory exception, reply is null");
    }
}

RedisReply::RedisReply(redisReply *reply) :
    m_reply(reply)
{
}

RedisReply::~RedisReply()
{
    freeReplyObject(m_reply);
}

redisReply *RedisReply::getContext()
{
    return m_reply;
}

redisReply *RedisReply::getChild(size_t index)
{
    if (index >= m_reply->elements)
    {
        throw std::out_of_range("Out of the range of redisReply elements");
    }
    return m_reply->element[index];
}

void RedisReply::checkStatus(char *status)
{
    if (strcmp(m_reply->str, status) != 0)
    {
        SWSS_LOG_ERROR("Redis reply %s != %s", m_reply->str, status);

        throw system_error(make_error_code(errc::io_error),
                           "Invalid return code");
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

template<> std::string RedisReply::getReply<std::string>()
{
    char *s = getContext()->str;
    if (s == NULL) return std::string();
    return std::string(s);
}

}
