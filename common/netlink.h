#ifndef __NETLINK__
#define __NETLINK__

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include "selectable.h"

namespace swss {

class NetLink : public Selectable {
public:
    NetLink(int pri = 0);
    ~NetLink() override;

    void registerGroup(int rtnlGroup);
    void dumpRequest(int rtmGetCommand);

    int getFd() override;
    uint64_t readData() override;

private:
    static int onNetlinkMsg(struct nl_msg *msg, void *arg);

    nl_sock *m_socket;
};

}

#endif
