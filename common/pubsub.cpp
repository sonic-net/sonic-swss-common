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
    printf("@@before RedisSelect::psubscribe\n");
    RedisSelect::psubscribe(m_parentConnector, pattern);
    printf("@@after RedisSelect::psubscribe\n");
    m_select.addSelectable(this);
    printf("@@after addSelectable\n");
}

uint64_t PubSub::readData()
{
    printf("@@enter readData\n");
    redisReply *reply = nullptr;

    /* Read data from redis. This call is non blocking. This method
     * is called from Select framework when data is available in socket.
     * NOTE: All data should be stored in event buffer. It won't be possible to
     * read them second time. */
    if (redisGetReply(m_subscribe->getContext(), reinterpret_cast<void**>(&reply)) != REDIS_OK)
    {
        throw std::runtime_error("Unable to read redis reply");
    }

    printf("@@readData: got one reply\n");
    m_keyspace_event_buffer.push_back(shared_ptr<RedisReply>(make_shared<RedisReply>(reply)));

    /* Try to read data from redis cacher.
     * If data exists put it to event buffer.
     * NOTE: Keyspace event is not persistent and it won't
     * be possible to read it second time. If it is not stared in
     * the buffer it will be lost. */

    reply = nullptr;
    int status;
    do
    {
        printf("@@readData: before redisGetReplyFromReader\n");
        status = redisGetReplyFromReader(m_subscribe->getContext(), reinterpret_cast<void**>(&reply));
        if(reply != nullptr && status == REDIS_OK)
        {
            printf("@@readData: after redisGetReplyFromReader, got another reply\n");
            m_keyspace_event_buffer.push_back(shared_ptr<RedisReply>(make_shared<RedisReply>(reply)));
        }
    }
    while(reply != nullptr && status == REDIS_OK);

    if (status != REDIS_OK)
    {
        printf("@@readData: before throw runtime_error\n");
        throw std::runtime_error("Unable to read redis reply");
    }
    printf("@@readData: total size=%zu\n", m_keyspace_event_buffer.size());
    return 0;
}

bool PubSub::hasData()
{
    printf("@@enter hasData\n");
    return m_keyspace_event_buffer.size() > 0;
}

bool PubSub::hasCachedData()
{
    printf("@@enter hasCachedData\n");
    return m_keyspace_event_buffer.size() > 1;
}

map<string, string> PubSub::get_message()
{
    printf("@@enter get_message\n");
    map<string, string> ret;
    if (!m_subscribe)
    {
        return ret;
    }

    Selectable *selected;
    int rc = m_select.select(&selected, 0);
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

    printf("@@after redisPollReply\n");
    printf("@@after RedisReply ctor\n");
    auto message = event->getReply<RedisMessage>();
    printf("@@after RedisReply.getReply()\n");
    ret["type"] = message.type;
    ret["pattern"] = message.pattern;
    ret["channel"] = message.channel;
    ret["data"] = message.data;
    return ret;
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
