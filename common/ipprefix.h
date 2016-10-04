#ifndef __IPPREFIX__
#define __IPPREFIX__

#include <assert.h>
#include <stdexcept>
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

    inline IpAddress getIp() const
    {
        return m_ip;
    }

    inline IpAddress getMask() const
    {
        int mask = m_mask < 0 ? 0 : m_mask;
        
        switch (m_ip.getIp().family)
        {
            case AF_INET:
            {
                if (mask > 32)
                    mask = 32;
                return IpAddress(htonl((0xFFFFFFFFLL << (32 - m_mask)) & 0xFFFFFFFF));
            }
            case AF_INET6:
            {
                ip_addr_t ipa;
                ipa.family = AF_INET6;
                
                // i : left_shift bits
                // 15: 128 - mask
                // 14: 120 - mask
                // 1 : 16  - mask
                // 0 : 8   - mask
                // n : n * 8 + 8 - mask
                // 
                // 0 <= n * 8 + 8 - mask < 8
                // mask / 8 - 1 <= n < mask / 8
                if (mask > 128)
                    mask = 128;
                int mid = (mask + 7) / 8 - 1;
                if (mid >= 0)
                {
                    int leftbit = (mask + 7) / 8 * 8 - mask;
                    ipa.ip_addr.ipv6_addr[mid] = 0xFF << leftbit;
                    memset(ipa.ip_addr.ipv6_addr, 0xFF, mid);
                }
                memset(ipa.ip_addr.ipv6_addr + mid + 1, 0, 16 - mid - 1);
                return IpAddress(ipa);
            }
            default:
            {
                throw std::logic_error("Invalid family");
            }
        }
    }

    inline int getMaskLength() const
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

    std::string to_string() const;

private:
    IpAddress m_ip;
    int m_mask;
};

}

#endif
