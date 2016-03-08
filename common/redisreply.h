#ifndef __REDISREPLY__
#define __REDISREPLY__

#include <hiredis/hiredis.h>
#include "dbconnector.h"

namespace swss {

class RedisReply
{
public:
    /*
     * Send a new command to redis and waits for reply
     * The reply must be one of REDIS_REPLY_* format (e.g. REDIS_REPLY_STATUS,
     * REDIS_REPLY_INTEGER, ...)
     * isFormatted - Set to true if the command is already formatted in redis
     *               protocol
     */
    RedisReply(DBConnector *db, std::string command, int exepectedType,
               bool isFormatted = false);

    /* auto_ptr for native structue (Free the memory on destructor) */
    RedisReply(redisReply *reply);

    /* Free the resources */
    ~RedisReply();

    /* Return the actual reply object */
    redisReply *getContext();

    /* Check that the staus is OK, throw exception otherwise */
    void checkStatusOK();

    /* Check that the staus is QUEUED, throw exception otherwise */
    void checkStatusQueued();

private:
    void checkStatus(char *status);

    redisReply *m_reply;
};

}
#endif
