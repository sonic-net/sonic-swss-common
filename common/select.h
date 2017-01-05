#ifndef __CONSUMERSELECT__
#define __CONSUMERSELECT__

#include <string>
#include <vector>
#include <limits>
#include <hiredis/hiredis.h>
#include "selectable.h"

namespace swss {

class Select
{
public:
    /* Add object for select */
    void addSelectable(Selectable *selectable);
    void removeSelectable(Selectable *selectable);
    void addSelectables(std::vector<Selectable *> selectables);

    /* Add file-descriptor for select  */
    void addFd(int fd);

    /*
     * Wait until data will arrived, returns the object on which select()
     * was signaled.
     */
    enum {
        OBJECT = 0,
        FD = 1,
        ERROR = 2,
        TIMEOUT = 3
    };
    int select(Selectable **c, int *fd, unsigned int timeout = std::numeric_limits<unsigned int>::max());

private:
    std::vector<Selectable * > m_objects;
    std::vector<int> m_fds;
};

}

#endif
