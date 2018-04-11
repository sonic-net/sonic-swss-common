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

    struct cmp
    {
        bool operator()(const Selectable* a, const Selectable* b) const
        {
            /* Choose Selectable with highest priority first */
            if (a->getPri() > b->getPri())
                return true;
            else if (a->getPri() < b->getPri())
                return false;

            /* if the priorities are equal */
            /* use Selectable which was selected later */
            if (a->getLastTimeUsed() < b->getLastTimeUsed())
                return true;
            else if (a->getLastTimeUsed() > b->getLastTimeUsed())
                return false;

            /* when a == b */
            return false;
        }
    };

private:
    int m_priority; // defines priority of Selectable inside Select
                    // higher value is higher priority
    std::chrono::time_point<std::chrono::steady_clock> m_last_time_used;
};

}

#endif
