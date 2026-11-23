#include "redissocket.h"
#include "redisauth.h"
#include "redisreply.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <system_error>

int swss::openRedisUnixSocket(const std::string& path, unsigned int timeout_ms, bool authenticate)
{
    if (path.empty() || path.front() != '/' || path.find('\0') != std::string::npos)
        throw std::invalid_argument("An absolute Redis Unix path is required");
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    const timeval timeout{static_cast<time_t>(timeout_ms / 1000), static_cast<suseconds_t>((timeout_ms % 1000) * 1000)};
    std::unique_ptr<redisContext, decltype(&redisFree)> context(timeout_ms
        ? redisConnectUnixWithTimeout(path.c_str(), timeout) : redisConnectUnix(path.c_str()), redisFree);
    if (!context) throw std::bad_alloc();
    if (context->err) throw RedisError("Redis Unix connection failed", context.get());
    if (authenticate)
    {
        if (timeout_ms)
        {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (left <= 0) throw std::system_error(std::make_error_code(std::errc::timed_out));
            const timeval remaining{static_cast<time_t>(left / 1000), static_cast<suseconds_t>((left % 1000) * 1000)};
            if (redisSetTimeout(context.get(), remaining) != REDIS_OK)
                throw RedisError("Cannot set AUTH timeout", context.get());
        }
        try { authenticateRedisConnection(context.get()); }
        catch (const RedisError&) {
            // A read that reaches the socket timeout fails with REDIS_ERR_TIMEOUT (hiredis
            // 1.0 and later), or with REDIS_ERR_IO and EAGAIN. The kernel can end that wait
            // slightly before the deadline, so the clock alone does not identify the timeout.
            const auto *c = context.get();
            const bool readTimedOut =
#ifdef REDIS_ERR_TIMEOUT
                c->err == REDIS_ERR_TIMEOUT ||
#endif
                (c->err == REDIS_ERR_IO && std::strcmp(c->errstr, std::strerror(EAGAIN)) == 0);
            if (timeout_ms && (readTimedOut || Clock::now() >= deadline))
                throw std::system_error(std::make_error_code(std::errc::timed_out));
            throw;
        }
        const timeval unlimited{};
        if (timeout_ms && redisSetTimeout(context.get(), unlimited) != REDIS_OK)
            throw RedisError("Cannot clear AUTH timeout", context.get());
    }
    const int fd = fcntl(context->fd, F_DUPFD_CLOEXEC, 0);
    if (fd < 0) throw std::system_error(errno, std::generic_category(), "Redis descriptor duplication");
    return fd;
}
