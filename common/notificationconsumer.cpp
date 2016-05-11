#include "notificationconsumer.h"

#include <iostream>

#define NOTIFICATION_SUBSCRIBE_TIMEOUT (1000)
#define REDIS_PUBLISH_MESSAGE_INDEX (2)
#define REDIS_PUBLISH_MESSAGE_ELEMNTS (3)

swss::NotificationConsumer::NotificationConsumer(swss::DBConnector *db, std::string channel):
    m_db(db), 
    m_subscribe(NULL), 
    m_channel(channel)
{
    SWSS_LOG_ENTER();

    while (true)
    {
        try
        {
            subscribe();
            break;
        }
        catch(...)
        {
            delete m_subscribe;

            SWSS_LOG_ERROR("failed to subscribe on %s", m_channel.c_str());
        }
    }
}

swss::NotificationConsumer::~NotificationConsumer()
{
    delete m_subscribe;
}

void swss::NotificationConsumer::subscribe()
{
    SWSS_LOG_ENTER();

    /* Create new new context to DB */
    if (m_db->getContext()->connection_type == REDIS_CONN_TCP)
        m_subscribe = new DBConnector(m_db->getDB(),
                                      m_db->getContext()->tcp.host,
                                      m_db->getContext()->tcp.port,
                                      NOTIFICATION_SUBSCRIBE_TIMEOUT);
    else
        m_subscribe = new DBConnector(m_db->getDB(),
                                      m_db->getContext()->unix_sock.path,
                                      NOTIFICATION_SUBSCRIBE_TIMEOUT);

    std::string s = "SUBSCRIBE " + m_channel;

    RedisReply r(m_subscribe, s, REDIS_REPLY_ARRAY);

    SWSS_LOG_INFO("subscribed to %s", m_channel.c_str());
}

void swss::NotificationConsumer::addFd(fd_set *fd)
{
    SWSS_LOG_ENTER();

    FD_SET(m_subscribe->getContext()->fd, fd);
}

int swss::NotificationConsumer::readCache()
{
    SWSS_LOG_ENTER();

    redisReply *reply = NULL;

    if (redisGetReplyFromReader(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
    {
        SWSS_LOG_ERROR("failed to read redis reply on channel %s", m_channel.c_str());

        return Selectable::ERROR;
    }
    else if (reply != NULL)
    {
        freeReplyObject(reply);

        return Selectable::DATA;
    }

    return Selectable::NODATA;
}

void swss::NotificationConsumer::readMe()
{
    SWSS_LOG_ENTER();

    redisReply *reply = NULL;

    if (redisGetReply(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
    {
        SWSS_LOG_ERROR("failed to read redis reply on channel %s", m_channel.c_str());

        throw std::runtime_error("Unable to read redis reply");
    }

    if (reply->type != REDIS_REPLY_ARRAY)
    {
        SWSS_LOG_ERROR("expected ARRAY redis reply on channel %s, got: %d", m_channel.c_str(), reply->type);

        throw std::runtime_error("getRedisReply operation failed");
    }

    if (reply->elements != REDIS_PUBLISH_MESSAGE_ELEMNTS)
    {
        SWSS_LOG_ERROR("expected %d elements in redis reply on channel %s, got: %u",
                       REDIS_PUBLISH_MESSAGE_ELEMNTS,
                       m_channel.c_str(),
                       reply->elements);

        throw std::runtime_error("getRedisReply operation failed");
    }

    m_msg = std::string(reply->element[REDIS_PUBLISH_MESSAGE_INDEX]->str);

    SWSS_LOG_DEBUG("got message: %s", m_msg.c_str());

    freeReplyObject(reply);
}

bool swss::NotificationConsumer::isMe(fd_set *fd)
{
    return FD_ISSET(m_subscribe->getContext()->fd, fd);
}

void swss::NotificationConsumer::pop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values)
{
    JSon::readJson(m_msg, values);

    FieldValueTuple fvt = values.at(0);

    op = fvField(fvt);
    data = fvValue(fvt);

    values.erase(values.begin());
}
