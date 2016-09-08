#ifndef __IPPREFIX__
#define __IPPREFIX__

#include <assert.h>
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
        switch (m_ip.getIp().family)
        {
            case AF_INET:
            {
                return IpAddress(htonl((0xFFFFFFFFLL << (32 - m_mask)) & 0xFFFFFFFF));
            }
            case AF_INET6:
            {
                ip_addr_t ipa;
                ipa.family = AF_INET6;
                
                // i : left_shift bits
                // 15: 128 - m_mask
                // 14: 120 - m_mask
                // 1 : 16  - m_mask
                // 0 : 8   - m_mask
                // n : n * 8 + 8 - m_mask
                // 
                // 0 <= n * 8 + 8 - m_mask < 8
                // m_mask / 8 - 1 <= n < m_mask / 8
                size_t mid = (m_mask + 7) / 8 - 1;
                size_t left = (m_mask + 7) / 8 * 8 - m_mask;
                ipa.ip_addr.ipv6_addr[mid] = 0xFF << left;
                memset(ipa.ip_addr.ipv6_addr, 0xFF, mid);
                memset(ipa.ip_addr.ipv6_addr + mid + 1, 0, 16 - mid - 1);
                return ipa;
            }
            default:
            {
                assert(false);
            }
        }
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

    const std::string to_string() const;

private:
    IpAddress m_ip;
    int m_mask;
};

}

#endif
