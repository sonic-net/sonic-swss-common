#include "notificationproducer.h"

#define NON_BUFFERED_COMMAND_BUFFER_SIZE 1

swss::NotificationProducer::NotificationProducer(swss::DBConnector *db, const std::string &channel):
    m_ownedpipe(std::make_unique<swss::RedisPipeline>(db, NON_BUFFERED_COMMAND_BUFFER_SIZE)), m_pipe(m_ownedpipe.get()), m_channel(channel), m_buffered(false)
{
}

swss::NotificationProducer::NotificationProducer(swss::RedisPipeline *pipeline, const std::string &channel, bool buffered):
     m_pipe(pipeline), m_channel(channel), m_buffered(buffered)
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

    RedisCommand command;
    command.format("PUBLISH %s %s", m_channel.c_str(), msg.c_str());

    if (m_buffered)
    {
        m_pipe->push(command, REDIS_REPLY_INTEGER);

        // if operating in buffered mode return -1 as we can't know the number
        // of client's that have read the message immediately
        return -1;
    }

    RedisReply reply = m_pipe->push(command);
    reply.checkReplyType(REDIS_REPLY_INTEGER);
    return reply.getReply<long long int>();
}
