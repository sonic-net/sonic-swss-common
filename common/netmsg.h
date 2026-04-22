#pragma once

#include <netlink/netlink.h>

namespace swss
{
    class NetMsg
    {
        public:
            /* Called by NetDispatcher when netmsg matches filters */
            virtual void onMsg(int nlmsg_type, struct nl_object *obj) = 0;

            /* Called by NetDispatcher when raw netlink msg matches filters.
             * Default implementation logs at DEBUG level. Subclasses that
             * register via registerRawMessageHandler should override this. */
            virtual void onMsgRaw(struct nlmsghdr *hdr);
    };
}
