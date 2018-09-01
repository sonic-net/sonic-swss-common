#ifndef __NOTIFICATIONCONSUMER__
#define __NOTIFICATIONCONSUMER__

#include <string>
#include <vector>
#include <queue>

#include <hiredis/hiredis.h>

#include "dbconnector.h"
#include "json.h"
#include "logger.h"
#include "redisreply.h"
#include "selectable.h"
#include "table.h"

namespace swss {

static constexpr size_t DEFAULT_NC_POP_BATCH_SIZE = 2048;

class NotificationConsumer : public Selectable
{
public:
    NotificationConsumer(swss::DBConnector *db, const std::string &channel, int pri = 100, size_t popBatchSize = DEFAULT_NC_POP_BATCH_SIZE);

    void pop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values);
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco);

    ~NotificationConsumer() override;

    int getFd() override;
    void readData() override;
    bool hasCachedData() override;
    const size_t POP_BATCH_SIZE;

private:

    NotificationConsumer(const NotificationConsumer &other);
    NotificationConsumer& operator = (const NotificationConsumer &other);

    void processReply(redisReply *reply);
    void subscribe();

    swss::DBConnector *m_db;
    swss::DBConnector *m_subscribe;
    std::string m_channel;
    std::queue<std::string> m_queue;
};

}

#endif // __NOTIFICATIONCONSUMER__

