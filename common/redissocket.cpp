#include "redissocket.h"
#include "c-api/internal/redissocket.h"

int swss::openRedisUnixSocket(const std::string& path, unsigned int timeout_ms, bool authenticate)
{
    return openRedisUnixSocketInterruptible(path, timeout_ms, authenticate, -1);
}
