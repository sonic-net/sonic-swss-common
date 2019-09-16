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

    void registerRecvCallbacks(void);
    void registerGroup(int nfnlGroup);
    void dumpRequest(int getCommand);

    int getFd() override;
    void readData() override;

    void updateConnTrackEntry(struct nfnl_ct *ct);
    void deleteConnTrackEntry(struct nfnl_ct *ct);

private:
    struct nfnl_ct *getCtObject(const IpAddress &sourceIpAddr);
    struct nfnl_ct *getCtObject(uint8_t protoType, const IpAddress &sourceIpAddr, uint16_t srcPort);

    static int onNetlinkRcv(struct nl_msg *msg, void *arg);
    static int onNetlinkMsg(struct nl_msg *msg, void *arg);

    FILE *nfPktsLogFile;
    nl_sock *m_socket;
};

}

#endif /* __NFNETLINK__ */

