#ifndef __SELECTABLEEVENT__
#define __SELECTABLEEVENT__

#include <string>
#include <vector>
#include <limits>
#include <sys/eventfd.h>
#include "selectable.h"

namespace swss {

class SelectableEvent : public Selectable
{
public:
    SelectableEvent();
    virtual ~SelectableEvent();

    void notify();

    virtual void addFd(fd_set *fd);
    virtual bool isMe(fd_set *fd);
    virtual int readCache();
    virtual void readMe();

private:
    int m_efd;
};

}

#endif // __SELECTABLEEVENT__
