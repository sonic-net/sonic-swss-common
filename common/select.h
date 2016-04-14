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
    void addSelectables(_in_ std::vector<Selectable *> &selectables);
    void addSelectable(_in_ Selectable *selectable);

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
    int select(_out_ Selectable **c, _out_ int *fd, _in_ unsigned int timeout = std::numeric_limits<unsigned int>::max());

private:
    /* Create a new redisContext, SELECT DB and SUBSRIBE */
    void subsribe();

    std::vector<Selectable * > m_objects;
    std::vector<int> m_fds;
};

}

#endif
