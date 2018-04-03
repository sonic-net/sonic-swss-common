#ifndef __CONSUMERSELECT__
#define __CONSUMERSELECT__

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <set>
#include <limits>
#include <hiredis/hiredis.h>
#include "selectable.h"

namespace swss {

class Select
{
public:
    Select();
    ~Select();

    /* Add object for select */
    void addSelectable(Selectable *selectable);

    /* Remove object from select */
    void removeSelectable(Selectable *selectable);

    /* Add multiple messages for select */
    void addSelectables(std::vector<Selectable *> selectables);

    enum {
        OBJECT = 0,
        ERROR = 1,
        TIMEOUT = 2,
    };

    int select(Selectable **c, int *fd, unsigned int timeout = std::numeric_limits<unsigned int>::max());

private:
    int select1(Selectable **c, unsigned int timeout);

    int m_epoll_fd;
    std::unordered_map<int, Selectable *> m_objects;
    std::set<Selectable *, Selectable::cmp> m_ready;
};

}

#endif
