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
    SelectableEvent(int pri = 0);
    virtual ~SelectableEvent();

    void notify();

    int getFd() override;
    void readData() override;

private:
    int m_efd;
};

}

#endif // __SELECTABLEEVENT__
