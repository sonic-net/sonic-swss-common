#include "common/netdispatcher.h"

#include "logger.h"

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

    if (!callback)
        throw "Trying to register a null callback";

    if (m_handlers.find(nlmsg_type) != m_handlers.end())
        throw "Trying to register on already registered netlink message";

    m_handlers[nlmsg_type] = callback;
}

void NetDispatcher::registerRawMessageHandler(int nlmsg_type, NetMsg *callback)
{
    MUTEX;

    if (!callback)
        throw "Trying to register a null callback";

    if (m_rawhandlers.find(nlmsg_type) != m_rawhandlers.end())
        throw "Trying to register an already registered netlink message";

    m_rawhandlers[nlmsg_type] = callback;
}

void NetDispatcher::unregisterMessageHandler(int nlmsg_type)
{
    MUTEX;

    auto it = m_handlers.find(nlmsg_type);

    if (it == m_handlers.end())
        throw "Trying to unregister non existing handler";

    m_handlers.erase(it);
}

void NetDispatcher::unregisterRawMessageHandler(int nlmsg_type)
{
    MUTEX;

    auto it = m_rawhandlers.find(nlmsg_type);

    if (it == m_rawhandlers.end())
        throw "Trying to unregister non existing handler";

    m_rawhandlers.erase(it);

}

void NetDispatcher::nlCallback(struct nl_object *obj, void *context)
{
    NetMsg *callback = (NetMsg *)context;
    callback->onMsg(nl_object_get_msgtype(obj), obj);
}

void NetDispatcher::onNetlinkMessageRaw(struct nl_msg *msg)
{
    struct nlmsghdr *nlmsghdr = nlmsg_hdr(msg);

    auto callback = getRawCallback(nlmsghdr->nlmsg_type);

    /* Drop not registered messages */
    if (callback == nullptr)
        return;

    callback->onMsgRaw(nlmsghdr);
}

NetMsg* NetDispatcher::getCallback(int nlmsg_type)
{
    MUTEX;

    auto callback = m_handlers.find(nlmsg_type);

    if (callback == m_handlers.end())
        return nullptr;

    return callback->second;
}

NetMsg* NetDispatcher::getRawCallback(int nlmsg_type)
{
    MUTEX;

    auto callback = m_rawhandlers.find(nlmsg_type);

    if (callback == m_rawhandlers.end())
        return nullptr;

    return callback->second;
}

void NetDispatcher::onNetlinkMessage(struct nl_msg *msg)
{
    struct nlmsghdr *nlmsghdr = nlmsg_hdr(msg);

    auto callback = getCallback(nlmsghdr->nlmsg_type);

    /* Raw handlers are only invoked when no parsed handler is registered.
     * If both a parsed handler and a raw handler are registered for the same
     * nlmsg_type, the raw handler will never fire. */
    if (callback == nullptr)
    {
        SWSS_LOG_DEBUG("No parsed handler for nlmsg_type %d, trying raw handler", nlmsghdr->nlmsg_type);
        onNetlinkMessageRaw(msg);
        return;
    }

    nl_msg_parse(msg, NetDispatcher::nlCallback, (callback));
}
