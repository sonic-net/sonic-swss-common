#include "pubsub.h"
#include "dbconnector.h"
#include "logger.h"
#include "redisreply.h"

using namespace std;
using namespace swss;

PubSub::PubSub(DBConnector *parent)
    : m_parentConnector(parent)
{
}

void PubSub::psubscribe(const std::string &pattern)
{
    if (m_subscribe)
    {
        m_select.removeSelectable(this);
    }
    RedisSelect::psubscribe(m_parentConnector, pattern);
    m_select.addSelectable(this);
}

void PubSub::punsubscribe(const std::string &pattern)
{
    RedisSelect::punsubscribe(pattern);
    m_select.removeSelectable(this);
}

uint64_t PubSub::readData()
{
    redisReply *reply = nullptr;

    /* Read data from redis. This call is non blocking. This method
     * is called from Select framework when data is available in socket.
     * NOTE: All data should be stored in event buffer. It won't be possible to
     * read them second time. */
    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
    {
        throw std::runtime_error("Unable to read redis reply");
    }

    m_keyspace_event_buffer.emplace_back(make_shared<RedisReply>(reply));

    /* Try to read data from redis cacher.
     * If data exists put it to event buffer.
     * NOTE: Keyspace event is not persistent and it won't
     * be possible to read it second time. If it is not stared in
     * the buffer it will be lost. */

    reply = nullptr;
    int status;
    do
    {
        status = redisGetReplyFromReader(m_subscribe->getContext(), reinterpret_cast<void**>(&reply));
        if(reply != nullptr && status == REDIS_OK)
        {
            m_keyspace_event_buffer.emplace_back(make_shared<RedisReply>(reply));
        }
    }
    while(reply != nullptr && status == REDIS_OK);

    if (status != REDIS_OK)
    {
        throw RedisError("Unable to redisGetReplyFromReader", m_subscribe->getContext());
    }
    return 0;
}

bool PubSub::hasData()
{
    return m_keyspace_event_buffer.size() > 0;
}

bool PubSub::hasCachedData()
{
    return m_keyspace_event_buffer.size() > 1;
}

map<string, string> PubSub::get_message(double timeout)
{
    map<string, string> ret;
    if (!m_subscribe)
    {
        return ret;
    }

    Selectable *selected;
    int rc = m_select.select(&selected, int(timeout));
    switch (rc)
    {
        case Select::ERROR:
            throw RedisError("Failed to select", m_subscribe->getContext());

        case Select::TIMEOUT:
            return ret;

        case Select::OBJECT:
            break;

        default:
            throw logic_error("Unexpected select result");
    }

    // Now we have some data to read
    auto event = popEventBuffer();
    if (!event)
    {
        return ret;
    }

    auto message = event->getReply<RedisMessage>();
    ret["type"] = message.type;
    ret["pattern"] = message.pattern;
    ret["channel"] = message.channel;
    ret["data"] = message.data;
    return ret;
}

// Note: it is not straightforward to implement redis-py PubSub.listen() directly in c++
// due to the `yield` syntax, so we implement this function for blocking listen one message
std::map<std::string, std::string> PubSub::listen_message()
{
    const double GET_MESSAGE_INTERVAL = 600.0; // in seconds
    for (;;)
    {
        auto ret = get_message(GET_MESSAGE_INTERVAL);
        if (!ret.empty())
        {
            return ret;
        }
    }
}

shared_ptr<RedisReply> PubSub::popEventBuffer()
{
    if (m_keyspace_event_buffer.empty())
    {
        return NULL;
    }

    auto reply = m_keyspace_event_buffer.front();
    m_keyspace_event_buffer.pop_front();

    return reply;
}
