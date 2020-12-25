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

const DBConnector* RedisSelect::getDbConnector() const
{
    return m_subscribe.get();
}

uint64_t RedisSelect::readData()
{
    redisReply *reply = nullptr;

    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
        throw std::runtime_error("Unable to read redis reply from RedisSelect::readData() redisGetReply()");

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
        throw std::runtime_error("Unable to read redis reply from RedisSelect::readData() redisGetReplyFromReader()");
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
    m_subscribe->subscribe(channelName);
}

/* PSUBSCRIBE */
void RedisSelect::psubscribe(DBConnector* db, const std::string &channelName)
{
    m_subscribe.reset(db->newConnector(SUBSCRIBE_TIMEOUT));
    /*
     * Send PSUBSCRIBE #channel command on the
     * non-blocking subscriber DBConnector
     */
    m_subscribe->psubscribe(channelName);
}

/* PUNSUBSCRIBE */
void RedisSelect::punsubscribe(const std::string &channelName)
{
    /*
     * Send PUNSUBSCRIBE #channel command on the
     * non-blocking subscriber DBConnector
     */
    if (m_subscribe)
    {
        m_subscribe->psubscribe(channelName);
    }
}

void RedisSelect::setQueueLength(long long int queueLength)
{
    m_queueLength = queueLength;
}

}
