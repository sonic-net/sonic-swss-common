#pragma once

#include <string>
#include "selectable.h"

namespace swss {

class RedisChannel : public RedisTransactioner
{
public:
    RedisChannel(DBConnector *db, std::string tableName) 
        : RedisTransactioner(db)
        , m_channelName(tableName + "_CHANNEL")
    {
    }

    std::string getChannelName()
    {
        return m_channelName;
    }

    void publish()
    {
        enqueue("PUBLISH " + getChannelName() + " G", REDIS_REPLY_INTEGER);
    }
    
private:
    std::string m_channelName;
};

class RedisSelect : public RedisChannel, public Selectable
{
public:
    /* The database is already alive and kicking, no need for more than a second */
    const static unsigned int SUBSCRIBE_TIMEOUT = 1000;
    
    RedisSelect(RedisChannel chan)
        : RedisChannel(chan)
        , m_subscribe(NULL)
    {
    }

    ~RedisSelect()
    {
        delete m_subscribe;
    }

    void addFd(fd_set *fd)
    {
        FD_SET(m_subscribe->getContext()->fd, fd);
    }

    int readCache()
    {
        redisReply *reply = NULL;

        /* Read the messages in queue before subsribe command execute */
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

        if (redisGetReply(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
            throw "Unable to read redis reply";

        freeReplyObject(reply);
    }

    bool isMe(fd_set *fd)
    {
        return FD_ISSET(m_subscribe->getContext()->fd, fd);
    }
    
    /* Create a new redisContext, SELECT DB and SUBSRIBE */
    void subscribe()
    {
        if (m_subscribe)
        {
            delete m_subscribe;
            m_subscribe = NULL;
        }
        
        /* Create new new context to DB */
        if (m_db->getContext()->connection_type == REDIS_CONN_TCP)
            m_subscribe = new DBConnector(m_db->getDB(),
                                          m_db->getContext()->tcp.host,
                                          m_db->getContext()->tcp.port,
                                          SUBSCRIBE_TIMEOUT);
        else
            m_subscribe = new DBConnector(m_db->getDB(),
                                          m_db->getContext()->unix_sock.path,
                                          SUBSCRIBE_TIMEOUT);
        /* Send SUBSCRIBE #channel command */
        std::string s("SUBSCRIBE ");
        s += getChannelName();
        RedisReply r(m_subscribe, s, REDIS_REPLY_ARRAY);
    }
    
    void setQueueLength(long long int queueLength)
    {
        m_queueLength = queueLength;
    }

private:
    DBConnector *m_subscribe;
    long long int m_queueLength;
};

}
