#pragma once
#include <map>
#include <deque>
#include <utility>

#include "dbconnector.h"
#include "select.h"
#include "redisselect.h"

namespace swss {

typedef std::pair<int, std::map<std::string, std::string> > MessageResultPair;

// This class is to emulate python redis-py class PubSub
// After SWIG wrapping, it should be used in the same way
class PubSub : protected RedisSelect
{
public:
    explicit PubSub(DBConnector *other);

    std::map<std::string, std::string> get_message(double timeout = 0.0, bool interrupt_on_signal = false);
    std::map<std::string, std::string> listen_message(bool interrupt_on_signal = false);

    void psubscribe(const std::string &pattern);
    void punsubscribe(const std::string &pattern);

    /* Read keyspace event from redis */
    uint64_t readData() override;
    bool hasData() override;
    bool hasCachedData() override;

private:
    /* Pop keyspace event from event buffer. Caller should free resources. */
    std::shared_ptr<RedisReply> popEventBuffer();
    MessageResultPair get_message_internal(double timeout = 0.0, bool interrupt_on_signal = false);

    DBConnector *m_parentConnector;
    Select m_select;
    std::deque<std::shared_ptr<RedisReply>> m_keyspace_event_buffer;
};

}
