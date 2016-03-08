#ifndef __NETLINK__
#define __NETLINK__

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include "selectable.h"

namespace swss {

class NetLink : public Selectable {
public:
    NetLink();
    virtual ~NetLink();

    void registerGroup(int rtnlGroup);
    void dumpRequest(int rtmGetCommand);

    virtual void addFd(fd_set *fd);
    virtual bool isMe(fd_set *fd);
    virtual int readCache();
    virtual void readMe();

private:
    static int onNetlinkMsg(struct nl_msg *msg, void *arg);

    nl_sock *m_socket;
};

}

#endif
