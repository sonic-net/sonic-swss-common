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
    virtual ~Selectable() {};

    enum {
        DATA = 0,
        ERROR = 1,
        NODATA = 2
    };

    /* Implements FD_SET */
    virtual void addFd(fd_set *fd) = 0;
    virtual bool isMe(fd_set *fd) = 0;

    /* Read and empty socket caching (if exists) */
    virtual int readCache() = 0;

    /* Read a message from the socket */
    virtual void readMe() = 0;
};

}

#endif
