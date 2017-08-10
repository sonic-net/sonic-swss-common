#include "common/selectable.h"
#include "common/logger.h"
#include "common/select.h"
#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

using namespace std;

namespace swss {

void Select::addSelectable(Selectable *selectable)
{
    if(find(m_objects.begin(), m_objects.end(), selectable) != m_objects.end())
    {
        SWSS_LOG_WARN("Selectable is already added to the list, ignoring.");
        return;
    }
    m_objects.push_back(selectable);
}

void Select::removeSelectable(Selectable *c)
{
    m_objects.erase(remove(m_objects.begin(), m_objects.end(), c), m_objects.end());
}

void Select::addSelectables(vector<Selectable *> selectables)
{
    for(auto it : selectables)
    {
        addSelectable(it);
    }
}

void Select::addFd(int fd)
{
    m_fds.push_back(fd);
}

int Select::select(Selectable **c, int *fd, unsigned int timeout)
{
    SWSS_LOG_ENTER();

    struct timeval t = {0, (suseconds_t)(timeout)*1000};
    struct timeval *pTimeout = NULL;
    fd_set fs;
    int err;

    FD_ZERO(&fs);
    *c = NULL;
    *fd = 0;
    if (timeout != numeric_limits<unsigned int>::max())
        pTimeout = &t;

    /* Checking caching from reader */
    for (Selectable *i : m_objects)
    {
        err = i->readCache();

        if (err == Selectable::ERROR)
            return Select::ERROR;
        else if (err == Selectable::DATA) {
            *c = i;
            return Select::OBJECT;
        }
        /* else, timeout = no data */

        i->addFd(&fs);
    }

    for (int fdn : m_fds)
    {
        FD_SET(fdn, &fs);
    }

    do
    {
        err = ::select(FD_SETSIZE, &fs, NULL, NULL, pTimeout);
    }
    while(err == -1 && errno == EINTR); // Retry the select if the process was interrupted by a signal

    if (err < 0)
        return Select::ERROR;
    if (err == 0)
        return Select::TIMEOUT;

    /* Check other consumer-table */
    for (Selectable *i : m_objects)
        if (i->isMe(&fs))
        {
            i->readMe();
            *c = i;
            return Select::OBJECT;
        }

    /* Check other FDs */
    for (int f : m_fds)
        if (FD_ISSET(f, &fs))
        {
            *fd = f;
            return Select::FD;
        }

    /* Shouldn't reach here */
    return Select::ERROR;
}

};
