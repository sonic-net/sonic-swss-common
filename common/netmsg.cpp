#include "netmsg.h"
#include "logger.h"

using namespace swss;

void NetMsg::onMsgRaw(struct nlmsghdr *hdr)
{
    SWSS_LOG_DEBUG("onMsgRaw called on base NetMsg class for nlmsg_type %d"
                   " - subclass should override this method", hdr->nlmsg_type);
}
