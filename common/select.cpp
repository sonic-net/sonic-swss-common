#include "common/selectable.h"
#include "common/logger.h"
#include "common/select.h"
#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

using namespace std;

namespace swss {

Select::Select()
{
    m_epoll_fd = ::epoll_create1(0);
    if (m_epoll_fd == -1)
    {
        std::string error = std::string("Select::constructor:epoll_create1: error=("
                          + std::to_string(errno) + "}:"
                          + strerror(errno));
        throw std::runtime_error(error);
    }
}

Select::~Select()
{
    (void)::close(m_epoll_fd);
}

void Select::addSelectable(Selectable *selectable)
{
    const int fd = selectable->getFd();

    if(m_objects.find(fd) != m_objects.end())
    {
        SWSS_LOG_WARN("Selectable is already added to the list, ignoring.");
        return;
    }

    m_objects[fd] = selectable;

    if (selectable->initializedWithData())
    {
        m_ready.insert(selectable);
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data = { .fd = fd, },
    };

    int res = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    if (res == -1)
    {
        std::string error = std::string("Select::add_fd:epoll_ctl: error=("
                          + std::to_string(errno) + "}:"
                          + strerror(errno));
        throw std::runtime_error(error);
    }
}

void Select::removeSelectable(Selectable *selectable)
{
    const int fd = selectable->getFd();

    m_objects.erase(fd);
    m_ready.erase(selectable);

    int res = ::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (res == -1)
    {
        std::string error = std::string("Select::del_fd:epoll_ctl: error=("
                          + std::to_string(errno) + "}:"
                          + strerror(errno));
        throw std::runtime_error(error);
    }
}

void Select::addSelectables(vector<Selectable *> selectables)
{
    for(auto it : selectables)
    {
        addSelectable(it);
    }
}

int Select::poll_descriptors(Selectable **c, unsigned int timeout)
{
    int sz_selectables = static_cast<int>(m_objects.size());
    std::vector<struct epoll_event> events(sz_selectables);
    int ret;

    do
    {
        ret = ::epoll_wait(m_epoll_fd, events.data(), sz_selectables, timeout);
    }
    while(ret == -1 && errno == EINTR); // Retry the select if the process was interrupted by a signal

    if (ret < 0)
        return Select::ERROR;

    for (int i = 0; i < ret; ++i)
    {
        int fd = events[i].data.fd;
        Selectable* sel = m_objects[fd];
        sel->readData();
        m_ready.insert(sel);
    }

    if (!m_ready.empty())
    {
        auto sel = *m_ready.begin();

        *c = sel;

        m_ready.erase(sel);
        // we must update clock only when the selector out of the m_ready
        // otherwise we break invariant of the m_ready
        sel->updateLastUsedTime();

        if (sel->hasCachedData())
        {
            // reinsert Selectable back to the m_ready set, when there're more messages in the cache
            m_ready.insert(sel);
        }

        sel->updateAfterRead();

        return Select::OBJECT;
    }

    return Select::TIMEOUT;
}

int Select::select(Selectable **c, unsigned int timeout)
{
    SWSS_LOG_ENTER();

    int ret;

    *c = NULL;
    if (timeout == numeric_limits<unsigned int>::max())
        timeout = -1;

    /* check if we have some data */
    ret = poll_descriptors(c, 0);

    /* return if we have data, we have an error or desired timeout was 0 */
    if (ret != Select::TIMEOUT || timeout == 0)
        return ret;

    /* wait for data */
    ret = poll_descriptors(c, timeout);

    return ret;

}

bool Select::isQueueNonEmpty()
{
    return !m_ready.empty();
}

};