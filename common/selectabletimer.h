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
    SelectableTimer(const timespec& interval, int pri = 50);
    ~SelectableTimer() override;
    void start();
    void stop();
    void reset();
    void setInterval(const timespec& interval);

    int getFd() override;
    uint64_t readData() override;

private:
    int m_tfd;
    itimerspec m_interval;
    itimerspec m_zero;
};

}
