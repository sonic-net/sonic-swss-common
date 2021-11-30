#ifndef __REDISREPLY__
#define __REDISREPLY__

#include <hiredis/hiredis.h>
#include <string>
#include <stdexcept>
#include "rediscommand.h"

namespace swss {

class RedisContext;

class RedisError : public std::runtime_error
{
    int m_err;
    std::string m_errstr;
    mutable std::string m_message;
public:
    RedisError(const std::string& arg, redisContext *ctx)
        : std::runtime_error(arg)
        , m_err(ctx->err)
        , m_errstr(ctx->errstr)
    {
    }

    const char *what() const noexcept override
    {
        if (m_message.empty())
        {
            m_message = std::string("RedisError: ") + std::runtime_error::what() + ", err=" + std::to_string(m_err) + ": errstr=" + m_errstr;
        }
        return m_message.c_str();
    }
};

// Follow the data structure used by redis-py
// ref: https://redis-py.readthedocs.io/en/stable/_modules/redis/client.html#Redis.pubsub
struct RedisMessage
{
    std::string type;
    std::string pattern;
    std::string channel;
    std::string data;
};

class RedisReply
{
public:
    /*
     * Send a new command to redis and wait for reply
     * No reply type specified.
     */
    RedisReply(RedisContext *ctx, const RedisCommand& command);
    RedisReply(RedisContext *ctx, const std::string& command);
    /*
     * Send a new command to redis and waits for reply
     * The reply must be one of REDIS_REPLY_* format (e.g. REDIS_REPLY_STATUS,
     * REDIS_REPLY_INTEGER, ...)
     * isFormatted - Set to true if the command is already formatted in redis
     *               protocol
     */
    RedisReply(RedisContext *ctx, const RedisCommand& command, int expectedType);
    RedisReply(RedisContext *ctx, const std::string& command, int expectedType);

    /* auto_ptr for native structure (Free the memory on destructor) */
    RedisReply(redisReply *reply);

    /* Free the resources */
    ~RedisReply();

    /* Return the actual reply object and release the ownership */
    redisReply *release();

    /* Return the actual reply object */
    redisReply *getContext();

    size_t getChildCount();

    redisReply *getChild(size_t index);

    /* Return the actual child reply object and release the ownership */
    redisReply *releaseChild(size_t index);

    void checkReplyType(int expectedType);

    template<typename TYPE>
    TYPE getReply();

    /* Check that the status is OK, throw exception otherwise */
    void checkStatusOK();

    /* Check that the status is QUEUED, throw exception otherwise */
    void checkStatusQueued();

private:
    void checkStatus(const char *status);
    void checkReply();

    redisReply *m_reply;
};

}
#endif
