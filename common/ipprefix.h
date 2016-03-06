#ifndef __IPPREFIX__
#define __IPPREFIX__

#include "ipaddress.h"

namespace swss {

class IpPrefix
{
public:
    IpPrefix() {}
    IpPrefix(const std::string &ipPrefixStr);
    IpPrefix(uint32_t addr, int mask);

    inline bool isV4() const
    {
        return m_ip.isV4();
    }

    inline const IpAddress getIp() const
    {
        return m_ip;
    }

    inline const IpAddress getMask() const
    {
        return IpAddress(htonl((0xFFFFFFFFLL << (32 - m_mask)) & 0xFFFFFFFF));
    }

    inline const int getMaskLength() const
    {
        return m_mask;
    }

    inline bool operator<(const IpPrefix &o) const
    {
        if (m_mask != o.m_mask)
            return m_mask < o.m_mask;
        else
            return m_ip < o.m_ip;
    }

    inline bool operator==(const IpPrefix &o) const
    {
        return m_ip == o.m_ip && m_mask == o.m_mask;
    }

    inline bool operator!=(const IpPrefix &o) const
    {
        return !(*this == o);
    }

    const std::string to_string() const;

private:
    IpAddress m_ip;
    int m_mask;
};

}

#endif
