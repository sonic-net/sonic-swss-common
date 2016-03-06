#include <string.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <system_error>

#include "common/logger.h"
#include "common/redisreply.h"

using namespace std;

namespace swss {

RedisReply::RedisReply(DBConnector *db, string command, int exepectedType, bool isFormatted)
{
    if (isFormatted)
    {
        redisAppendFormattedCommand(db->getContext(), command.c_str(), command.length());
        redisGetReply(db->getContext(), (void**)&m_reply);
    } else
    {
        m_reply = (redisReply *)redisCommand(db->getContext(), command.c_str());
    }

    if (!m_reply)
        throw system_error(make_error_code(errc::not_enough_memory),
                           "Memory exception");

    if (m_reply->type != exepectedType) {
        SWSS_LOG_INFO("Except to get redis type %d got type %d\n",
                      exepectedType, m_reply->type);
        freeReplyObject(m_reply);
        m_reply = NULL; /* Some compilers call destructor in this case */
        throw system_error(make_error_code(errc::io_error),
                           "Wrong expected type of result");
    }
}

RedisReply::RedisReply(redisReply *reply) :
    m_reply(reply)
{
}

RedisReply::~RedisReply()
{
    if (m_reply)
        freeReplyObject(m_reply);
}

redisReply *RedisReply::getContext()
{
    return m_reply;
}

void RedisReply::checkStatus(char *status)
{
    if (strcmp(m_reply->str, status) != 0)
        throw system_error(make_error_code(errc::io_error),
                           "Invalid return code");
}

void RedisReply::checkStatusOK()
{
    checkStatus("OK");
}

void RedisReply::checkStatusQueued()
{
    checkStatus("QUEUED");
}

}
