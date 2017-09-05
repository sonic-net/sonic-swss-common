#ifndef __REDISREPLYASYNC__
#define __REDISREPLYASYNC__

#include <hiredis/hiredis.h>
#include <stdexcept>
#include "dbconnector.h"
#include "rediscommand.h"

namespace swss {

class RedisReplyAsync
{
public:
    /*
     * Send a new command to redis and wait for reply
     * No reply type specified.
     */
    RedisReplyAsync(DBConnector *db, const RedisCommand& command);
    RedisReplyAsync(DBConnector *db, std::string command);
    /*
     * Send a new command to redis and waits for reply
     * The reply must be one of REDIS_REPLY_* format (e.g. REDIS_REPLY_STATUS,
     * REDIS_REPLY_INTEGER, ...)
     * isFormatted - Set to true if the command is already formatted in redis
     *               protocol
     */
    RedisReplyAsync(DBConnector *db, const RedisCommand& command, int expectedType);
    RedisReplyAsync(DBConnector *db, std::string command, int expectedType);

    /*
     * To read the async reply from subscribing request.
     */
    RedisReplyAsync(DBConnector *db);

    /* Free the resources */
    ~RedisReplyAsync();

    /* Return the actual reply object */
    redisReply *getContext();

    /* Check that the staus is OK, throw exception otherwise */
    void checkStatusOK();

private:
    void checkStatus(char *status);
    void checkReply();
    void checkReplyType(int expectedType);
    void nonBlockingRedisReply(DBConnector *db);

    redisReply *m_reply;
    DBConnector *m_db;
};

}
#endif
