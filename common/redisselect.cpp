#include <string>
#include <memory>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "redisreply.h"
#include "selectable.h"
#include "redisselect.h"

namespace swss {

RedisSelect::RedisSelect()
{
}

void RedisSelect::addFd(fd_set *fd)
{
    FD_SET(m_subscribe->getContext()->fd, fd);
}

int RedisSelect::readCache()
{
    redisReply *reply = NULL;

    /* Read the messages in queue before subscribe command execute */
    if (m_queueLength) {
        m_queueLength--;
        return Selectable::DATA;
    }

    if (redisGetReplyFromReader(m_subscribe->getContext(),
                (void**)&reply) != REDIS_OK)
    {
        return Selectable::ERROR;
    } else if (reply != NULL)
    {
        freeReplyObject(reply);
        return Selectable::DATA;
    }

    return Selectable::NODATA;
}

void RedisSelect::readMe()
{
    redisReply *reply = NULL;

    if (redisGetReply(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
        throw "Unable to read redis reply";

    freeReplyObject(reply);
}

bool RedisSelect::isMe(fd_set *fd)
{
    return FD_ISSET(m_subscribe->getContext()->fd, fd);
}

/* Create a new redisContext, SELECT DB and SUBSCRIBE */
void RedisSelect::subscribe(DBConnector* db, std::string channelName)
{
    m_subscribe.reset(db->newConnector(SUBSCRIBE_TIMEOUT));

    /* Send SUBSCRIBE #channel command */
    std::string s("SUBSCRIBE ");
    s += channelName;
    RedisReply r(m_subscribe.get(), s, REDIS_REPLY_ARRAY);
}

/* PSUBSCRIBE */
void RedisSelect::psubscribe(DBConnector* db, std::string channelName)
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
