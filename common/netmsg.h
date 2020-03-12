#pragma once

#include <netlink/netlink.h>

namespace swss
{
    class NetMsg
    {
        public:
            /* Called by NetDispatcher when netmsg matches filters */
            virtual void onMsg(int nlmsg_type, struct nl_object *obj) = 0;
    };
}
