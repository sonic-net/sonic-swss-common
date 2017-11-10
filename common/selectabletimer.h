#pragma once

#include <string>
#include <vector>
#include <limits>
#include <sys/timerfd.h>
#include "selectable.h"

namespace swss {

class SelectableTimer : public Selectable
{
public:
    SelectableTimer(const timespec& interval);
    virtual ~SelectableTimer();

    virtual void addFd(fd_set *fd);
    virtual bool isMe(fd_set *fd);
    virtual int readCache();
    virtual void readMe();

private:
    int m_tfd;
};

}
