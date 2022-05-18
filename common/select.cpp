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

    std::set<Selectable *, Select::cmp> temp_ready;
    for (int i = 0; i < ret; ++i)
    {
        int fd = events[i].data.fd;
        Selectable* sel = m_objects[fd];
        try
        {
            sel->readData();
        }
        catch (const std::runtime_error& ex)
        {
            SWSS_LOG_ERROR("readData error: %s", ex.what());
            return Select::ERROR;
        }
        temp_ready.insert(sel);
    }

    bool dataReady = false;
    foreach (auto sel : temp_ready)
    {
        // we must update clock only when the selector out of the m_ready
        // otherwise we break invariant of the m_ready
        sel->updateLastUsedTime();

        if (!sel->hasData())
        {
            continue;
        }

        // SWIG generated code will only allocate 1 Selectable pointer on stack and pass to select() method as 1st parameter
        // So here can only assign first Selectable pointer to c
        if (!dataReady)
        {
            *c = sel;
        }

        if (sel->hasCachedData())
        {
            // insert Selectable to the m_ready set, when there're more messages in the cache
            m_ready.insert(sel);
        }

        sel->updateAfterRead();
        dataReady = true;
    }

    return dataReady ? Select::OBJECT :Select::TIMEOUT;
}

int Select::select(Selectable **c, int timeout)
{
    SWSS_LOG_ENTER();

    int ret;

    *c = NULL;

    /* check if we have some data */
    ret = poll_descriptors(c, 0);

    /* return if we have data, we have an error or desired timeout was 0 */
    if (ret != Select::TIMEOUT || timeout == 0)
        return ret;

    /* wait for data */
    ret = poll_descriptors(c, timeout);

    return ret;

}

bool Select::isQueueEmpty()
{
    return m_ready.empty();
}

std::string Select::resultToString(int result)
{
    SWSS_LOG_ENTER();

    switch (result)
    {
        case swss::Select::OBJECT:
            return "OBJECT";

        case swss::Select::ERROR:
            return "ERROR";

        case swss::Select::TIMEOUT:
            return "TIMEOUT";

        default:
            SWSS_LOG_WARN("unknown select result: %d", result);
            return "UNKNOWN";
    }
}

};
