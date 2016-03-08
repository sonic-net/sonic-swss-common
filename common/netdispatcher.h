#ifndef __NETDISPATCHER__
#define __NETDISPATCHER__

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/utils.h>
#include <netlink/data.h>
#include <netlink/route/rtnl.h>

#include <map>

#include "netmsg.h"

namespace swss {

class NetDispatcher {
public:
    /**/
    static NetDispatcher& getInstance();

    /*
     * Register callback class according to message-type.
     *
     * Throw exception if
     */
    void registerMessageHandler(int nlmsg_type, NetMsg *callback);

    /*
     * Called by NetLink or FpmLink classes as indication of new packet arrival
     */
    void onNetlinkMessage(struct nl_msg *msg);

private:
    NetDispatcher();
    ~NetDispatcher();

    /* nl_msg_parse callback API */
    static void nlCallback(struct nl_object *obj, void *context);

    std::map<int, NetMsg * > m_handlers;
};

}

#endif
