#ifndef __REDISREPLY__
#define __REDISREPLY__

#include <hiredis/hiredis.h>
#include <string>
#include <stdexcept>
#include <system_error>
#include "logger.h"
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

    std::string to_string();

    static std::string to_string(redisReply *reply, std::string command = std::string());

private:
    void checkStatus(const char *status);
    void checkReply();

    static std::string formatReply(std::string command, struct redisReply **element, size_t elements);
    static std::string formatReply(std::string command, long long integer);
    static std::string formatReply(std::string command, const char* str, size_t len);
    static std::string formatSscanReply(struct redisReply **element, size_t elements);
    static std::string formatHscanReply(struct redisReply **element, size_t elements);
    static std::string formatDictReply(struct redisReply **element, size_t elements);
    static std::string formatArrayReply(struct redisReply **element, size_t elements);
    static std::string formatListReply(struct redisReply **element, size_t elements);
    static std::string formatTupleReply(struct redisReply **element, size_t elements);
    static std::string formatStringWithQuot(std::string str);

    redisReply *m_reply;
};

template <typename FUNC>
inline void guard(FUNC func, const char* command)
{
    try
    {
        func();
    }
    catch (const std::system_error& ex)
    {
        // Combine more error message and throw again
        std::string reason = ex.what();
        std::string errmsg = "RedisReply catches system_error: command: " + std::string(command) + ", reason: " + reason;
        // C++ does not have a startswith function. To ensure the search begins at the start of the string 'reason', pass pos = 0
        if (reason.rfind("LOADING ", 0) == 0)
        {
            // The command will fail when Redis is loading the dataset.
            SWSS_LOG_WARN("%s", errmsg.c_str());
        }
        else
        {
            SWSS_LOG_ERROR("%s", errmsg.c_str());
        }
        throw std::system_error(ex.code(), errmsg.c_str());
    }
}

}
#endif
