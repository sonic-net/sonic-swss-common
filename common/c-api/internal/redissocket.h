#pragma once
#include <string>
namespace swss
{
// Single budget and cancellation during connect and AUTH, for native dialers. cancelFd -1 disables cancellation.
int openRedisUnixSocketInterruptible(const std::string& path, unsigned int timeout_ms,
                                     bool authenticate, int cancelFd);
}
