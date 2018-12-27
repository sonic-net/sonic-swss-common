#include "notificationconsumer.h"

#include <iostream>
#include <deque>
#include "redisapi.h"

#define NOTIFICATION_SUBSCRIBE_TIMEOUT (1000)
#define REDIS_PUBLISH_MESSAGE_INDEX (2)
#define REDIS_PUBLISH_MESSAGE_ELEMNTS (3)

swss::NotificationConsumer::NotificationConsumer(swss::DBConnector *db, const std::string &channel, int pri, size_t popBatchSize):
    Selectable(pri),
    POP_BATCH_SIZE(popBatchSize),
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
        m_subscribe = new DBConnector(m_db->getDbId(),
                                      m_db->getContext()->tcp.host,
                                      m_db->getContext()->tcp.port,
                                      NOTIFICATION_SUBSCRIBE_TIMEOUT);
    else
        m_subscribe = new DBConnector(m_db->getDbId(),
                                      m_db->getContext()->unix_sock.path,
                                      NOTIFICATION_SUBSCRIBE_TIMEOUT);

    std::string s = "SUBSCRIBE " + m_channel;

    RedisReply r(m_subscribe, s, REDIS_REPLY_ARRAY);

    SWSS_LOG_INFO("subscribed to %s", m_channel.c_str());
}

int swss::NotificationConsumer::getFd()
{
    return m_subscribe->getContext()->fd;
}

void swss::NotificationConsumer::readData()
{
    SWSS_LOG_ENTER();

    redisReply *reply = nullptr;

    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
    {
        SWSS_LOG_ERROR("failed to read redis reply on channel %s", m_channel.c_str());

        throw std::runtime_error("Unable to read redis reply");
    }
    else
    {
        RedisReply r(reply);
        processReply(reply);
    }

    reply = nullptr;
    int status;
    do
    {
        status = redisGetReplyFromReader(m_subscribe->getContext(), reinterpret_cast<void**>(&reply));
        if(reply != nullptr && status == REDIS_OK)
        {
            RedisReply r(reply);
            processReply(reply);
        }
    }
    while(reply != nullptr && status == REDIS_OK);

    if (status != REDIS_OK)
    {
        throw std::runtime_error("Unable to read redis reply");
    }
}

bool swss::NotificationConsumer::hasCachedData()
{
    return m_queue.size() > 1;
}

void swss::NotificationConsumer::processReply(redisReply *reply)
{
    SWSS_LOG_ENTER();

    if (reply->type != REDIS_REPLY_ARRAY)
    {
        SWSS_LOG_ERROR("expected ARRAY redis reply on channel %s, got: %d", m_channel.c_str(), reply->type);

        throw std::runtime_error("getRedisReply operation failed");
    }

    if (reply->elements != REDIS_PUBLISH_MESSAGE_ELEMNTS)
    {
        SWSS_LOG_ERROR("expected %d elements in redis reply on channel %s, got: %zu",
                       REDIS_PUBLISH_MESSAGE_ELEMNTS,
                       m_channel.c_str(),
                       reply->elements);

        throw std::runtime_error("getRedisReply operation failed");
    }

    std::string msg = std::string(reply->element[REDIS_PUBLISH_MESSAGE_INDEX]->str);

    SWSS_LOG_DEBUG("got message: %s", msg.c_str());

    m_queue.push(msg);
}

void swss::NotificationConsumer::pop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    if (m_queue.empty())
    {
        SWSS_LOG_ERROR("notification queue is empty, can't pop");
        throw std::runtime_error("notification queue is empty, can't pop");
    }

    std::string msg = m_queue.front();
    m_queue.pop();

    JSon::readJson(msg, values);

    FieldValueTuple fvt = values.at(0);

    op = fvField(fvt);
    data = fvValue(fvt);

    values.erase(values.begin());
}

void swss::NotificationConsumer::pops(std::deque<KeyOpFieldsValuesTuple> &vkco)
{
    SWSS_LOG_ENTER();
    std::string op;
    std::string data;
    std::vector<FieldValueTuple> values;

    vkco.clear();
    while(!m_queue.empty())
    {
        while(!m_queue.empty())
        {
            pop(op, data, values);
            vkco.emplace_back(data, op, values);
        }

        // Too many popped, let's return to prevent DoS attach
        if (vkco.size() >= POP_BATCH_SIZE)
            return;

        // Peek for more data in redis socket
        int rc = swss::peekRedisContext(m_subscribe->getContext());
        if (rc <= 0)
            break;

        readData();
    }
}

int swss::NotificationConsumer::peek()
{
    SWSS_LOG_ENTER();
    if (m_queue.empty())
    {
        // Peek for more data in redis socket
        int rc = swss::peekRedisContext(m_subscribe->getContext());
        if (rc <= 0)
            return rc;

        // Feed into internal queue
        readData();
    }

    return m_queue.empty() ? 0 : 1;
}
