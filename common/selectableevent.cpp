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

SelectableEvent::SelectableEvent(int pri) :
    Selectable(pri), m_efd(0)
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

int SelectableEvent::getFd()
{
    return m_efd;
}

void SelectableEvent::readData()
{
    uint64_t r;
    ssize_t s;

    uint8_t* r_ptr = reinterpret_cast<uint8_t*>(&r);
    size_t bytes_to_read = sizeof(uint64_t);

    do
    {
        s = read(m_efd, r_ptr, bytes_to_read);
        if (s > 0)
        {
            r_ptr += s;
            bytes_to_read -= s;
        }
    }
    while ((s == -1 && errno == EINTR) || (bytes_to_read > 0));

    if (s == -1)
    {
        SWSS_LOG_THROW("SelectableEvent read failed, errno: %d '%s'", errno, strerror(errno));
    }
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
