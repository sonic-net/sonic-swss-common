#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <system_error>

#include "common/logger.h"
#include "common/selectableevent.h"

namespace swss {

SelectableEvent::SelectableEvent() :
    m_efd(0)
{
    m_efd = eventfd(0, 0);

    if (m_efd == -1)
    {
        SWSS_LOG_ERROR("failed to create eventfd, errno: %s", strerror(errno));

        throw std::runtime_error("failed to create eventfd");
    }
}

SelectableEvent::~SelectableEvent()
{
    int err;

    do
    {
        err = close(m_efd);
    }
    while(err == -1 && errno == EINTR);
}

void SelectableEvent::addFd(fd_set *fd)
{
    FD_SET(m_efd, fd);
}

int SelectableEvent::readCache()
{
    return Selectable::NODATA;
}

void SelectableEvent::readMe()
{
    uint64_t r;

    ssize_t s;
    do
    {
        s = read(m_efd, &r, sizeof(uint64_t));
    }
    while(s == -1 && errno == EINTR);

    if (s != sizeof(uint64_t))
    {
        SWSS_LOG_ERROR("read failed, errno: %s", strerror(errno));

        throw std::runtime_error("read failed");
    }
}

bool SelectableEvent::isMe(fd_set *fd)
{
    return FD_ISSET(m_efd, fd);
}

void SelectableEvent::notify()
{
    SWSS_LOG_ENTER();

    uint64_t value = 1;
    ssize_t s;
    do
    {
        s = write(m_efd, &value, sizeof(uint64_t));
    }
    while(s == -1 && errno == EINTR);

    if (s != sizeof(uint64_t))
    {
        SWSS_LOG_ERROR("write failed, errno: %s", strerror(errno));

        throw std::runtime_error("write failed");
    }
}

}
