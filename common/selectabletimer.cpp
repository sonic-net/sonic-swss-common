#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <system_error>

#include "common/logger.h"
#include "common/selectabletimer.h"

namespace swss {

SelectableTimer::SelectableTimer(const timespec& interval)
{
    // Create the timer
    m_tfd = timerfd_create(CLOCK_REALTIME, 0);
    if (m_tfd == -1)
    {
        SWSS_LOG_ERROR("failed to create timerfd, errno: %s", strerror(errno));
        throw std::runtime_error("failed to create timerfd");
    }

    // The initial expiration and intervals to caller specified
    struct itimerspec interv;
    interv.it_value = interval;
    interv.it_interval = interval;

    // Set the timer interval
    int rc = timerfd_settime(m_tfd, 0, &interv, NULL);
    if (rc == -1)
    {
        SWSS_LOG_ERROR("failed to set timerfd, errno: %s", strerror(errno));
        throw std::runtime_error("failed to set timerfd");
    }
}

SelectableTimer::~SelectableTimer()
{
    int err;

    do
    {
        err = close(m_tfd);
    }
    while(err == -1 && errno == EINTR);
}

void SelectableTimer::addFd(fd_set *fd)
{
    FD_SET(m_tfd, fd);
}

int SelectableTimer::readCache()
{
    return Selectable::NODATA;
}

void SelectableTimer::readMe()
{
    uint64_t r;

    ssize_t s;
    do
    {
        // Read the timefd so it will be reset
        s = read(m_tfd, &r, sizeof(uint64_t));
    }
    while(s == -1 && errno == EINTR);

    if (s != sizeof(uint64_t))
    {
        SWSS_LOG_ERROR("read failed, errno: %s", strerror(errno));

        throw std::runtime_error("read failed");
    }
}

bool SelectableTimer::isMe(fd_set *fd)
{
    return FD_ISSET(m_tfd, fd);
}

}
