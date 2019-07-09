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

SelectableTimer::SelectableTimer(const timespec& interval, int pri)
    : Selectable(pri), m_zero({{0, 0}, {0, 0}})
{
    // Create the timer
    m_tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (m_tfd == -1)
    {
        SWSS_LOG_THROW("failed to create timerfd, errno: %s", strerror(errno));
    }
    setInterval(interval);
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

void SelectableTimer::start()
{
    // Set the timer interval and the timer is automatically started
    int rc = timerfd_settime(m_tfd, 0, &m_interval, NULL);
    if (rc == -1)
    {
        SWSS_LOG_THROW("failed to set timerfd, errno: %s", strerror(errno));
    }
}

void SelectableTimer::stop()
{
    // Set the timer interval and the timer is automatically started
    int rc = timerfd_settime(m_tfd, 0, &m_zero, NULL);
    if (rc == -1)
    {
        SWSS_LOG_THROW("failed to set timerfd to zero, errno: %s", strerror(errno));
    }
}

void SelectableTimer::reset()
{
    stop();
    start();
}

void SelectableTimer::setInterval(const timespec& interval)
{
    // The initial expiration and intervals to caller specified
    m_interval.it_value = interval;
    m_interval.it_interval = interval;
}

int SelectableTimer::getFd()
{
    return m_tfd;
}

void SelectableTimer::readData()
{
    uint64_t r = UINT64_MAX;

    ssize_t s;
    errno = 0;
    do
    {
        s = read(m_tfd, &r, sizeof(uint64_t));
    }
    while(s == -1 && errno == EINTR);

    /*
     * By right or most likely s = 8 here.
     * Else in failure case, s = -1 with errno != 0, b
     * But due to a (HW influenced) Kernel bug, it could be s=0 & errno=0
     *
     * This bug has been observed in S6100 only.
     * The bug incidence is pretty seldom.
     *
     * A short note on kernel bug:
     *  timerfd_read, upon asserting that underlying hrtimer has triggered once,
     *  calls hrtimer_forward_now, to get total count of expiries since timer start
     *  to now, as read gets called asynchronously at a time later than the time
     *  point of trigger.
     *  The hrtimer_forward_now is expected to return >= 1, as it is certain
     *  that one trigger/expiry is confirmed to have occurred.
     *  But in the buggy case, the hrtimer_forward_now returned 0, that results
     *  in read call to return 0.
     *  
     *  The behavior of read returning 0, with errno=0 is an unexpected behavior.
     */
    ABORT_IF_NOT((s == sizeof(uint64_t)) || ((s == 0) && (errno == 0)),
            "Failed to read timerfd. s=%ld", s)

    if (s != sizeof(uint64_t)) {
        SWSS_LOG_ERROR("Benign failure to read from timerfd return=%d error=%s",
                s, strerror(errno));
    }

    // r = count of timer events happened since last read.
}

}
