#include "redissocket.h"
#include "redisauth.h"
#include "redisreply.h"
#include <chrono>
#include <fcntl.h>
#include <memory>
#include <system_error>

namespace
{
using Clock = std::chrono::steady_clock;

void checkDeadline(Clock::time_point deadline)
{
    if (Clock::now() >= deadline)
        throw std::system_error(std::make_error_code(std::errc::timed_out));
}

void setAuthTimeout(redisContext *context, Clock::time_point deadline)
{
    const auto budget = deadline - Clock::now();
    if (budget <= Clock::duration::zero())
        throw std::system_error(std::make_error_code(std::errc::timed_out));
    // Round up so a socket timeout cannot precede the overall deadline.
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(budget).count() + 1;
    const timeval remaining{static_cast<time_t>(left / 1000), static_cast<suseconds_t>((left % 1000) * 1000)};
    if (redisSetTimeout(context, remaining) != REDIS_OK)
        throw swss::RedisError("Cannot set AUTH timeout", context);
}

redisReply *authenticateWithDeadline(redisContext *context, const char **args, const size_t *lengths, void *opaque)
{
    const auto deadline = *static_cast<Clock::time_point *>(opaque);
    checkDeadline(deadline);
    if (redisAppendCommandArgv(context, 3, args, lengths) != REDIS_OK)
        throw swss::RedisError("Cannot queue Redis authentication", context);
    int flushed = 0;
    while (!flushed)
    {
        setAuthTimeout(context, deadline);
        if (redisBufferWrite(context, &flushed) != REDIS_OK)
            throw swss::RedisError("Redis authentication write failure", context);
    }
    for (;;)
    {
        checkDeadline(deadline);
        void *reply = nullptr;
        if (redisGetReplyFromReader(context, &reply) != REDIS_OK)
            throw swss::RedisError("Redis authentication reply failure", context);
        if (reply) return static_cast<redisReply *>(reply);
        setAuthTimeout(context, deadline);
        if (redisBufferRead(context) != REDIS_OK)
            throw swss::RedisError("Redis authentication read failure", context);
    }
}
}

int swss::openRedisUnixSocket(const std::string& path, unsigned int timeout_ms, bool authenticate)
{
    if (path.empty() || path.front() != '/' || path.find('\0') != std::string::npos)
        throw std::invalid_argument("An absolute Redis Unix path is required");
    auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    const timeval timeout{static_cast<time_t>(timeout_ms / 1000), static_cast<suseconds_t>((timeout_ms % 1000) * 1000)};
    std::unique_ptr<redisContext, decltype(&redisFree)> context(timeout_ms
        ? redisConnectUnixWithTimeout(path.c_str(), timeout) : redisConnectUnix(path.c_str()), redisFree);
    if (!context) throw std::bad_alloc();
    if (timeout_ms) checkDeadline(deadline);
    if (context->err) throw RedisError("Redis Unix connection failed", context.get());
    if (authenticate)
    {
        try {
            if (timeout_ms)
                authenticateRedisConnection(context.get(), authenticateWithDeadline, &deadline);
            else
                authenticateRedisConnection(context.get());
        }
        catch (const RedisError&) {
            if (timeout_ms) checkDeadline(deadline);
            throw;
        }
        const timeval unlimited{};
        if (timeout_ms && redisSetTimeout(context.get(), unlimited) != REDIS_OK)
            throw RedisError("Cannot clear AUTH timeout", context.get());
    }
    if (timeout_ms) checkDeadline(deadline);
    const int fd = fcntl(context->fd, F_DUPFD_CLOEXEC, 0);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "Redis descriptor duplication");
    return fd;
}
