#ifndef __LINKCACHE__
#define __LINKCACHE__

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/utils.h>
#include <netlink/data.h>
#include <netlink/route/rtnl.h>
#include <netlink/route/link.h>
#include <string>

namespace swss {

class LinkCache {
public:
    static LinkCache &getInstance();

    /* Translate ifindex to name */
    std::string ifindexToName(int ifindex);

private:
    LinkCache();
    ~LinkCache();

    nl_cache *m_link_cache;
    nl_sock *m_nl_sock;
};

}

#endif
