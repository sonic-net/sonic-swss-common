#include "notificationproducer.h"

swss::NotificationProducer::NotificationProducer(swss::DBConnector *db, std::string channel):
    m_db(db), m_channel(channel)
{
}

void swss::NotificationProducer::send(std::string op, std::string data, std::vector<FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    FieldValueTuple opdata(op, data);

    values.insert(values.begin(), opdata);

    std::string msg = JSon::buildJson(values);

    values.erase(values.begin());

    SWSS_LOG_DEBUG("channel %s, publish: %s", m_channel.c_str(), msg.c_str());

    char *temp;
    int len = redisFormatCommand(&temp, "PUBLISH %s %s", m_channel.c_str(), msg.c_str());
    std::string publish(temp, len);
    free(temp);

    RedisReply r(m_db, publish, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
    {
        SWSS_LOG_ERROR("PUBLISH %s '%s' failed, got type %d, expected %d",
                       m_channel.c_str(),
                       msg.c_str(),
                       r.getContext()->type,
                       REDIS_REPLY_INTEGER);

        throw std::runtime_error("PUBLISH operation failed");
    }
}
