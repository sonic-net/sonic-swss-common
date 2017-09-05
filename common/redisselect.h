#pragma once

#include <string>
#include <memory>
#include "selectable.h"
#include "redisreplyasync.h"

namespace swss {

class RedisSelect : public Selectable
{
public:
    /* The database is already alive and kicking, no need for more than a second */
    const static unsigned int SUBSCRIBE_TIMEOUT = 1000;

    RedisSelect()
    {
    }

    void addFd(fd_set *fd)
    {
        FD_SET(m_subscribe->getContext()->fd, fd);
    }

    int readCache()
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

    void readMe()
    {
        redisReply *reply = NULL;

        /*
         * For non-blocking redis connection, the reply has data
         * to be processed later.
         */
        if(!m_subscribe->isBlocking())
            return;
        if (redisGetReply(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
            throw "Unable to read redis reply";

        freeReplyObject(reply);
    }

    bool isMe(fd_set *fd)
    {
        return FD_ISSET(m_subscribe->getContext()->fd, fd);
    }

    /* Create a new redisContext, SELECT DB and SUBSCRIBE */
    void subscribe(DBConnector* db, std::string channelName)
    {
        m_subscribe.reset(db->newConnector(SUBSCRIBE_TIMEOUT));

        /* Send SUBSCRIBE #channel command */
        std::string s("SUBSCRIBE ");
        s += channelName;
        RedisReply r(m_subscribe.get(), s, REDIS_REPLY_ARRAY);
    }

    /* PSUBSCRIBE */
    void psubscribe(DBConnector* db, std::string channelName)
    {
        m_subscribe.reset(db->newConnector());

        /*
         * Send PSUBSCRIBE #channel command on the
         * non-blocking subscriber DBConnector
         */
        std::string s("PSUBSCRIBE ");
        s += channelName;
        RedisReplyAsync r(m_subscribe.get(), s, REDIS_REPLY_ARRAY);
    }

    /* return the subscribe DB connector */
    DBConnector* getSubscribeDBC()
    {
        return m_subscribe.get();
    }

    void setQueueLength(long long int queueLength)
    {
        m_queueLength = queueLength;
    }

private:
    std::unique_ptr<DBConnector> m_subscribe;
    long long int m_queueLength;
};

}
