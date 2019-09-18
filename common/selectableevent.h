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
    ~SelectableEvent() override;

    void notify();

    int getFd() override;
    uint64_t readData() override;

private:
    int m_efd;
};

}

#endif // __SELECTABLEEVENT__
