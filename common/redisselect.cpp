#include <string>
#include <memory>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "redisreply.h"
#include "selectable.h"
#include "redisselect.h"

namespace swss {

RedisSelect::RedisSelect(int pri) : Selectable(pri), m_queueLength(-1)
{
}

int RedisSelect::getFd()
{
    return m_subscribe->getContext()->fd;
}
int RedisSelect::getDbConnectorId()
{
    return m_subscribe->getDbId();
}

std::string RedisSelect::getDbNamespace()
{
    return m_subscribe->getNamespace();
}

uint64_t RedisSelect::readData()
{
    redisReply *reply = nullptr;

    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
        throw std::runtime_error("Unable to read redis reply");

    freeReplyObject(reply);
    m_queueLength++;

    reply = nullptr;
    int status;
    do
    {
        status = redisGetReplyFromReader(m_subscribe->getContext(), reinterpret_cast<void**>(&reply));
        if(reply != nullptr && status == REDIS_OK)
        {
            m_queueLength++;
            freeReplyObject(reply);
        }
    }
    while(reply != nullptr && status == REDIS_OK);

    if (status != REDIS_OK)
    {
        throw std::runtime_error("Unable to read redis reply");
    }
    return 0;
}

bool RedisSelect::hasData()
{
    return m_queueLength > 0;
}

bool RedisSelect::hasCachedData()
{
    return m_queueLength > 1;
}

bool RedisSelect::initializedWithData()
{
    return m_queueLength > 0;
}

void RedisSelect::updateAfterRead()
{
    m_queueLength--;
}

/* Create a new redisContext, SELECT DB and SUBSCRIBE */
void RedisSelect::subscribe(DBConnector* db, const std::string &channelName)
{
    m_subscribe.reset(db->newConnector(SUBSCRIBE_TIMEOUT));

    /* Send SUBSCRIBE #channel command */
    std::string s("SUBSCRIBE ");
    s += channelName;
    RedisReply r(m_subscribe.get(), s, REDIS_REPLY_ARRAY);
}

/* PSUBSCRIBE */
void RedisSelect::psubscribe(DBConnector* db, const std::string &channelName)
{
    m_subscribe.reset(db->newConnector(SUBSCRIBE_TIMEOUT));

    /*
     * Send PSUBSCRIBE #channel command on the
     * non-blocking subscriber DBConnector
     */
    std::string s("PSUBSCRIBE ");
    s += channelName;
    RedisReply r(m_subscribe.get(), s, REDIS_REPLY_ARRAY);
}

void RedisSelect::setQueueLength(long long int queueLength)
{
    m_queueLength = queueLength;
}

}
