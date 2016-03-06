#include <map>
#include "common/netdispatcher.h"
#include "common/netmsg.h"

using namespace swss;

NetDispatcher::NetDispatcher()
{
}

NetDispatcher::~NetDispatcher()
{
}

NetDispatcher& NetDispatcher::getInstance()
{
    static NetDispatcher gInstance;
    return gInstance;
}

void NetDispatcher::registerMessageHandler(int nlmsg_type, NetMsg *callback)
{
    if (m_handlers.find(nlmsg_type) != m_handlers.end())
        throw "Trying to registered on already registerd netlink message";

    m_handlers[nlmsg_type] = callback;
}

void NetDispatcher::nlCallback(struct nl_object *obj, void *context)
{
    NetMsg *callback = (NetMsg *)context;
    callback->onMsg(nl_object_get_msgtype(obj), obj);
}

void NetDispatcher::onNetlinkMessage(struct nl_msg *msg)
{
    struct nlmsghdr *nlmsghdr = nlmsg_hdr(msg);
    auto callback = m_handlers.find(nlmsghdr->nlmsg_type);

    /* Drop not registered messages */
    if (callback == m_handlers.end())
        return;

    nl_msg_parse(msg, NetDispatcher::nlCallback, (void *)(callback->second));
}
