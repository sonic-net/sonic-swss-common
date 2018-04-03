#ifndef __SELECT__
#define __SELECT__

#include <string>
#include <vector>
#include <limits>
#include <hiredis/hiredis.h>

namespace swss {

class Selectable
{
public:
    Selectable(int pri = 0) : m_priority(pri) {}
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

    struct cmp
    {
        bool operator()(const Selectable* a, const Selectable* b) const
        {
            if (a->getPri() == b->getPri())
                return a > b;
            else
                return a->getPri() > b->getPri();
        }
    };

private:
    int m_priority;
};

}

#endif
