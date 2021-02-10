#include "common/netdispatcher.h"

#include <map>

using namespace swss;

#define MUTEX std::lock_guard<std::mutex> _lock(m_mutex);

NetDispatcher& NetDispatcher::getInstance()
{
    static NetDispatcher gInstance;

    return gInstance;
}

void NetDispatcher::registerMessageHandler(int nlmsg_type, NetMsg *callback)
{
    MUTEX;

    if (m_handlers.find(nlmsg_type) != m_handlers.end())
        throw "Trying to register on already registered netlink message";

    m_handlers[nlmsg_type] = callback;
}

void NetDispatcher::unregisterMessageHandler(int nlmsg_type)
{
    MUTEX;

    auto it = m_handlers.find(nlmsg_type);

    if (it == m_handlers.end())
        throw "Trying to unregister non existing handler";

    m_handlers.erase(it);
}

void NetDispatcher::nlCallback(struct nl_object *obj, void *context)
{
    NetMsg *callback = (NetMsg *)context;
    callback->onMsg(nl_object_get_msgtype(obj), obj);
}

NetMsg* NetDispatcher::getCallback(int nlmsg_type)
{
    MUTEX;

    auto callback = m_handlers.find(nlmsg_type);

    if (callback == m_handlers.end())
        return nullptr;

    return callback->second;
}

void NetDispatcher::onNetlinkMessage(struct nl_msg *msg)
{
    struct nlmsghdr *nlmsghdr = nlmsg_hdr(msg);

    auto callback = getCallback(nlmsghdr->nlmsg_type);

    /* Drop not registered messages */
    if (callback == nullptr)
        return;

    nl_msg_parse(msg, NetDispatcher::nlCallback, (callback));
}
