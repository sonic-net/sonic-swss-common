#include <string.h>
#include <errno.h>
#include <system_error>
#include <netlink/route/link.h>
#include "common/logger.h"
#include "common/linkcache.h"

using namespace std;
using namespace swss;

LinkCache::LinkCache()
{
    m_nl_sock = nl_socket_alloc();
    if (!m_nl_sock)
    {
        SWSS_LOG_ERROR("Unable to allocated netlink socket");
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to allocated netlink socket");
    }

    int err = nl_connect(m_nl_sock, NETLINK_ROUTE);
    if (err < 0)
    {
        SWSS_LOG_ERROR("Unable to connect netlink socket: %s", nl_geterror(err));
        nl_socket_free(m_nl_sock);
        m_nl_sock = NULL;
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect netlink socket");
    }

    err = rtnl_link_alloc_cache(m_nl_sock, AF_UNSPEC, &m_link_cache);
    if (err < 0)
    {
        SWSS_LOG_ERROR("Unable to allocate link cache: %s", nl_geterror(err));
        nl_close(m_nl_sock);
        nl_socket_free(m_nl_sock);
        m_nl_sock = NULL;
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect netlink socket");
    }
}

LinkCache::~LinkCache()
{
    if (m_nl_sock != NULL)
    {
        nl_close(m_nl_sock);
        nl_socket_free(m_nl_sock);
    }
}

LinkCache &LinkCache::getInstance()
{
    static LinkCache linkCache;
    return linkCache;
}

string LinkCache::ifindexToName(int ifindex)
{
#define MAX_ADDR_SIZE 128
    char addrStr[MAX_ADDR_SIZE + 1] = {0};

    if (rtnl_link_i2name(m_link_cache, ifindex, addrStr, MAX_ADDR_SIZE) == NULL)
    {
        /* Trying to refill cache */
        nl_cache_refill(m_nl_sock ,m_link_cache);
        if (rtnl_link_i2name(m_link_cache, ifindex,
                             addrStr, MAX_ADDR_SIZE) == NULL)
        {
            /* Returns ifindex as string / */
            return to_string(ifindex);
        }

    }

    return string(addrStr);
}
