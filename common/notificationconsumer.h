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

    // Pop one or multiple data from the internal queue which fed from redis socket
    // Note:
    //    Ensure data ready before popping, either by select or peek
    void pop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values);
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco);

    // Check the internal queue which fed from redis socket for data ready
    // Returns:
    //     1 - data immediately available inside internal queue, may be just fed from redis socket
    //     0 - no data both in internal queue or redis socket
    //    -1 - error during peeking redis socket
    int peek();

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

