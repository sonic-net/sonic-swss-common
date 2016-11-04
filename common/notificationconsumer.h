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

class NotificationConsumer : public Selectable
{
public:
    NotificationConsumer(swss::DBConnector *db, std::string channel);

    void pop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values);

    virtual ~NotificationConsumer();

    virtual void addFd(fd_set *fd);
    virtual bool isMe(fd_set *fd);
    virtual int readCache();
    virtual void readMe();

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

