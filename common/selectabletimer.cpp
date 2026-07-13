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
    m_running = false;
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
    m_mutex.lock();
    if (!m_running)
    {
        // Set the timer interval and the timer is automatically started
        int rc = timerfd_settime(m_tfd, 0, &m_interval, NULL);
        if (rc == -1)
        {
            m_mutex.unlock();
            SWSS_LOG_THROW("failed to set timerfd, errno: %s", strerror(errno));
        }
        else
        {
            m_running = true;
        }
    }
    m_mutex.unlock();
}

void SelectableTimer::stop()
{
    m_mutex.lock();
    if (m_running)
    {
        // Set the timer interval and the timer is automatically started
        int rc = timerfd_settime(m_tfd, 0, &m_zero, NULL);
        if (rc == -1)
        {
            m_mutex.unlock();
            SWSS_LOG_THROW("failed to set timerfd to zero, errno: %s", strerror(errno));
        }
        else
        {
            m_running = false;
        }
    }
    m_mutex.unlock();
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

uint64_t SelectableTimer::readData()
{
    uint64_t cnt = 0;

    ssize_t ret;
    errno = 0;
    do
    {
        ret = read(m_tfd, &cnt, sizeof(uint64_t));
    }
    while(ret == -1 && errno == EINTR);

    ABORT_IF_NOT((ret == 0) || (ret == sizeof(uint64_t)), "Failed to read timerfd. ret=%zd", ret);

    // cnt = count of timer events happened since last read.
    return cnt;
}

}
