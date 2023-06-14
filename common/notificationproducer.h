#ifndef __NOTIFICATIONPRODUCER__
#define __NOTIFICATIONPRODUCER__

#include <string>
#include <vector>

#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "logger.h"
#include "table.h"
#include "redisreply.h"
#include "redispipeline.h"
#include "json.h"

namespace swss {

class NotificationProducer
{
public:
    NotificationProducer(swss::DBConnector *db, const std::string &channel);

    /**
     * @brief Create NotificationProducer using RedisPipeline
     * @param pipeline Pointer to RedisPipeline
     * @param channel Channel name
     * @param buffered Whether NotificationProducer will work in buffered mode
     */
    NotificationProducer(RedisPipeline *pipeline, const std::string &channel, bool buffered = false);

    // Returns: the number of clients that received the message
    int64_t send(const std::string &op, const std::string &data, std::vector<FieldValueTuple> &values);

private:

    NotificationProducer(const NotificationProducer &other);
    NotificationProducer& operator = (const NotificationProducer &other);

    std::unique_ptr<RedisPipeline> m_ownedpipe{};
    RedisPipeline *m_pipe;
    std::string m_channel;
    bool m_buffered{false};
};

}

#endif // __NOTIFICATIONPRODUCER__
