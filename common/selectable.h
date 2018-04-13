#ifndef __SELECT__
#define __SELECT__

#include <string>
#include <vector>
#include <limits>
#include <chrono>
#include <hiredis/hiredis.h>

namespace swss {

class Selectable
{
public:
    Selectable(int pri = 0) : m_priority(pri),
                              m_last_time_used(std::chrono::steady_clock::now()) {}

    virtual ~Selectable() {};

    /* return file handler for the Selectable */
    virtual int getFd() = 0;

    /* Read all data from the fd assicaited with Selectable */
    virtual void readData() = 0;

    /* true if Selectable has data in its cache */
    virtual bool hasCachedData()
    {
        return false;
    }

    /* true if Selectable was initialized with data */
    virtual bool initializedWithData()
    {
        return false;
    }

    /* run this function after every read */
    virtual void updateAfterRead()
    {
    }

    int getPri() const
    {
        return m_priority;
    }

    std::chrono::time_point<std::chrono::steady_clock> getLastTimeUsed() const
    {
        return m_last_time_used;
    }

    void updateClock()
    {
        m_last_time_used = std::chrono::steady_clock::now();
    }

private:
    int m_priority; // defines priority of Selectable inside Select
                    // higher value is higher priority
    std::chrono::time_point<std::chrono::steady_clock> m_last_time_used;
};

}

#endif
