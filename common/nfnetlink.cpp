#include <string.h>
#include <errno.h>
#include <system_error>
#include "common/logger.h"
#include "common/netmsg.h"
#include "common/netdispatcher.h"
#include "common/netlink.h"
#include "common/nfnetlink.h"

using namespace swss;
using namespace std;

NfNetlink::NfNetlink(int pri) :
    Selectable(pri)
{
    m_socket = nl_socket_alloc();
    if (!m_socket)
    {
        SWSS_LOG_ERROR("Unable to allocated nfnetlink socket");
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to allocate nfnetlink socket");
    }

    nl_socket_disable_seq_check(m_socket);
    nl_socket_disable_auto_ack(m_socket);

    int err = nfnl_connect(m_socket);
    if (err < 0)
    {
        SWSS_LOG_ERROR("Unable to connect nfnetlink socket: %s", nl_geterror(err));
        nl_socket_free(m_socket);
        m_socket = NULL;
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect nfnetlink socket");
    }

    nl_socket_set_nonblocking(m_socket);

    /* Set socket buffer size to 10 MB */
    nl_socket_set_buffer_size(m_socket, 10485760, 0);

#ifdef NETFILTER_UNIT_TEST
    /* For netlink packet tracing purposes */
    nfPktsLogFile = fopen("/var/log/nf_netlink_pkts.log", "w");
    if (nfPktsLogFile == NULL)
    {
        SWSS_LOG_ERROR("Unable to open netfilter netlink packets log file: %s", strerror(errno));
    }
#endif
}

NfNetlink::~NfNetlink()
{
    if (m_socket != NULL)
    {
        nl_close(m_socket);
        nl_socket_free(m_socket);
    }
}

bool NfNetlink::setSockBufSize(uint32_t sockBufSize)
{
    if (nl_socket_set_buffer_size(m_socket, sockBufSize, 0) < 0)
    {
        return false;
    }
    return true;
}

void NfNetlink::registerRecvCallbacks()
{
#ifdef NETFILTER_UNIT_TEST
    nl_socket_modify_cb(m_socket, NL_CB_MSG_IN, NL_CB_CUSTOM, onNetlinkRcv, this);
#endif

    nl_socket_modify_cb(m_socket, NL_CB_VALID, NL_CB_CUSTOM, onNetlinkMsg, this);
}

void NfNetlink::registerGroup(int nfnlGroup)
{
    int err = nl_socket_add_membership(m_socket, nfnlGroup);
    if (err < 0)
    {
        SWSS_LOG_THROW("Unable to register to netfilter group %d: %s", nfnlGroup,
                       nl_geterror(err));
    }
}

void NfNetlink::dumpRequest(int getCommand)
{
    int err = nfnl_ct_dump_request(m_socket);
    if (err < 0)
    {
        SWSS_LOG_THROW("Unable to request netfilter conntrack dump: %s",
                       nl_geterror(err));
    }
}

bool NfNetlink::updateConnTrackEntry(struct nfnl_ct *ct)
{
    if (nfnl_ct_add(m_socket, ct, NLM_F_REPLACE) < 0)
    {
        SWSS_LOG_ERROR("Failed to update conntrack object in the kernel");
        return false;
    }
    return true;
}

bool NfNetlink::deleteConnTrackEntry(struct nfnl_ct *ct)
{
    if (nfnl_ct_del(m_socket, ct, 0) < 0)
    {
        SWSS_LOG_ERROR("Failed to delete conntrack object in the kernel");
        return false;
    }
    return true;
}

int NfNetlink::getFd()
{
    return nl_socket_get_fd(m_socket);
}

uint64_t NfNetlink::readData()
{
    int err;

    do
    {
        err = nl_recvmsgs_default(m_socket);
    }
    while(err == -NLE_INTR); // Retry if the process was interrupted by a signal

    if (err < 0)
    {
        if (err == -NLE_NOMEM)
            SWSS_LOG_ERROR("netlink reports out of memory on reading a netfilter netlink socket. High possibility of a lost message");
        else if (err == -NLE_AGAIN)
            SWSS_LOG_DEBUG("netlink reports NLE_AGAIN on reading a netfilter netlink socket");
        else
            SWSS_LOG_ERROR("netlink reports an error=%d on reading a netfilter netlink socket", err);
    }
    return 0;
}

int NfNetlink::onNetlinkMsg(struct nl_msg *msg, void *arg)
{
    NetDispatcher::getInstance().onNetlinkMessage(msg);
    return NL_OK;
}

#ifdef NETFILTER_UNIT_TEST
int NfNetlink::onNetlinkRcv(struct nl_msg *msg, void *arg)
{
    NfNetlink  *nfnlink = (NfNetlink *)arg;

    if ((nfnlink == NULL) || (nfnlink->nfPktsLogFile == NULL))
      return NL_OK;

    nl_msg_dump(msg, nfnlink->nfPktsLogFile);

    return NL_OK;
}
#endif
