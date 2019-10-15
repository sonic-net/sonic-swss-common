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

uint64_t RedisSelect::readData()
{
    redisReply *reply = nullptr;

    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
        throw std::runtime_error("Unable to read redis reply");

    freeReplyObject(reply);
    m_queueLength++;

    readRemainingData();
}

/* Read remaining messages in Redis buffer nonblockingly */
void RedisSelect::readRemainingData()
{
    redisReply *reply = nullptr;
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

/* Discard messages in the channel */
void RedisSelect::discard(long long int n)
{
    readRemainingData();

    /*
     * We will discard at least one message, to prevent any mistakenly infinite loop
     * in the select-pop(s) pattern
     */
    if (n <= 0)
        n = 1;

    if (n > m_queueLength)
    {
        /* If we have less messages, discard them all
         * This is no big harm since all the late messages will be selected and nothing popped
         * when the channel is not completely busy.
         */
        m_queueLength = 0;
    }
    else
    {
        /* Otherwise discard as requested by n */
        m_queueLength -= n;
    }
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
