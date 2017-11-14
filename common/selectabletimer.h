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
    void start();
    void stop();
    void setInterval(const timespec& interval);

    virtual void addFd(fd_set *fd);
    virtual bool isMe(fd_set *fd);
    virtual int readCache();
    virtual void readMe();

private:
    int m_tfd;
    itimerspec m_interval;
    itimerspec m_zero;
};

}
