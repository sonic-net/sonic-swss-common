#pragma once

#include <netlink/netlink.h>
#include <netlink/route/link.h>

#include <string>

namespace swss {

class LinkCache {
public:
    static LinkCache &getInstance();

    /* Translate ifindex to name */
    std::string ifindexToName(int ifindex);
    struct rtnl_link* getLinkByName(const char* name);

private:
    LinkCache();
    ~LinkCache();

    nl_cache *m_link_cache;
    nl_sock *m_nl_sock;
};

}
