#ifndef __NETMSG__
#define __NETMSG__

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/utils.h>
#include <netlink/data.h>
#include <netlink/route/rtnl.h>

namespace swss {

class NetMsg {
public:
    /* Called by NetDispatcher when netmsg matches filters */
    virtual void onMsg(int nlmsg_type, struct nl_object *obj) = 0;
};

}

#endif
