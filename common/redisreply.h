#ifndef __REDISREPLY__
#define __REDISREPLY__

#include <hiredis/hiredis.h>
#include <stdexcept>
#include "dbconnector.h"
#include "rediscommand.h"

namespace swss {

class RedisReply
{
public:
    /*
     * Send a new command to redis and wait for reply
     * No reply type specified.
     */
    RedisReply(DBConnector *db, const RedisCommand& command);
    RedisReply(DBConnector *db, std::string command);
    /*
     * Send a new command to redis and waits for reply
     * The reply must be one of REDIS_REPLY_* format (e.g. REDIS_REPLY_STATUS,
     * REDIS_REPLY_INTEGER, ...)
     * isFormatted - Set to true if the command is already formatted in redis
     *               protocol
     */
    RedisReply(DBConnector *db, const RedisCommand& command, int expectedType);
    RedisReply(DBConnector *db, std::string command, int expectedType);

    /* auto_ptr for native structue (Free the memory on destructor) */
    RedisReply(redisReply *reply);

    /* Free the resources */
    ~RedisReply();

    /* Return the actual reply object and release the ownership */
    redisReply *release();

    /* Return the actual reply object */
    redisReply *getContext();

    redisReply *getChild(size_t index);

    void checkReplyType(int expectedType);

    template<typename TYPE>
    TYPE getReply();

    /* Check that the staus is OK, throw exception otherwise */
    void checkStatusOK();

    /* Check that the staus is QUEUED, throw exception otherwise */
    void checkStatusQueued();

private:
    void checkStatus(char *status);
    void checkReply();

    redisReply *m_reply;
};

}
#endif
