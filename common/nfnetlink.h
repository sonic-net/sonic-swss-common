#ifndef __NFNETLINK__
#define __NFNETLINK__

#include "ipaddress.h"
#include <netlink/netfilter/ct.h>
#include <netlink/netfilter/nfnl.h>

namespace swss {

class NfNetlink : public Selectable {
public:
    NfNetlink(int pri = 0);
    ~NfNetlink() override;

    void registerRecvCallbacks();
    bool setSockBufSize(uint32_t sockBufSize);
    void registerGroup(int nfnlGroup);
    void dumpRequest(int getCommand);

    int getFd() override;
    uint64_t readData() override;

    bool updateConnTrackEntry(struct nfnl_ct *ct);
    bool deleteConnTrackEntry(struct nfnl_ct *ct);

private:
#ifdef NETFILTER_UNIT_TEST
    static int onNetlinkRcv(struct nl_msg *msg, void *arg);
#endif
    static int onNetlinkMsg(struct nl_msg *msg, void *arg);

    FILE *nfPktsLogFile;
    nl_sock *m_socket;
};

}

#endif /* __NFNETLINK__ */

