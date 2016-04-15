#include <algorithm>
#include "common/selectable.h"
#include "common/logger.h"
#include "common/select.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

using namespace std;

namespace swss {

void Select::addSelectable(Selectable *selectable)
{
    if(find(m_objects.begin(), m_objects.end(), selectable) != m_objects.end()) {
        SWSS_LOG_WARN("Selectable:%p already been added to the list, ignoring.", selectable);
        return;
    }
    m_objects.push_back(selectable);
}

void Select::addSelectables(vector<Selectable *> selectables)
{
    for(auto it : selectables) {
        addSelectable(it);
    }    
}

void Select::addFd(int fd)
{
    m_fds.push_back(fd);
}

int Select::select(Selectable **c, int *fd, unsigned int timeout)
{
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

    for (int fd : m_fds)
    {
        FD_SET(fd, &fs);
    }

    err = ::select(FD_SETSIZE, &fs, NULL, NULL, pTimeout);
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
