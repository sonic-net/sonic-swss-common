#include "notificationproducer.h"

swss::NotificationProducer::NotificationProducer(swss::DBConnector *db, const std::string &channel):
    m_db(db), m_channel(channel)
{
}

int64_t swss::NotificationProducer::send(const std::string &op, const std::string &data, std::vector<FieldValueTuple> &values)
{
    SWSS_LOG_ENTER();

    FieldValueTuple opdata(op, data);

    values.insert(values.begin(), opdata);

    std::string msg = JSon::buildJson(values);

    values.erase(values.begin());

    SWSS_LOG_DEBUG("channel %s, publish: %s", m_channel.c_str(), msg.c_str());

    return m_db->publish(m_channel, msg);
}
