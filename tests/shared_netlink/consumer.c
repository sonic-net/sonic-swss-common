#include "swss/c-api/util.h"

#include <netlink/netfilter/ct.h>
#include <netlink/route/link.h>
#include <netlink/socket.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    SWSSString value = SWSSString_new_c_str("shared-netlink-consumer");
    struct nl_sock *socket = nl_socket_alloc();
    struct rtnl_link *link = rtnl_link_alloc();
    struct nfnl_ct *connection = nfnl_ct_alloc();
    if (value == NULL || socket == NULL || link == NULL || connection == NULL)
    {
        fprintf(stderr, "Common/libnl object allocation failed\n");
        return 1;
    }
    int matches = strcmp(SWSSStrRef_c_str((SWSSStrRef)value),
                         "shared-netlink-consumer") == 0;
    SWSSString_free(value);
    nl_socket_free(socket);
    rtnl_link_put(link);
    nfnl_ct_put(connection);
    if (!matches)
    {
        fprintf(stderr, "Common string roundtrip failed\n");
        return 1;
    }
    puts("Common and all three libnl shared libraries executed successfully");
    return 0;
}
