#pragma once

#include <string>
#include <memory>
#include "selectable.h"

namespace swss {

class RedisSelect : public Selectable
{
public:
    /* The database is already alive and kicking, no need for more than a second */
    static constexpr unsigned int SUBSCRIBE_TIMEOUT = 1000;

    RedisSelect();

    void addFd(fd_set *fd);

    int readCache();

    void readMe();

    bool isMe(fd_set *fd);

    /* Create a new redisContext, SELECT DB and SUBSCRIBE */
    void subscribe(DBConnector* db, std::string channelName);

    /* PSUBSCRIBE */
    void psubscribe(DBConnector* db, std::string channelName);

    void setQueueLength(long long int queueLength);

protected:
    std::unique_ptr<DBConnector> m_subscribe;
    long long int m_queueLength;
};

}
