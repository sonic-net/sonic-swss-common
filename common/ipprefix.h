#ifndef __IPPREFIX__
#define __IPPREFIX__

#include <assert.h>
#include <stdexcept>
#include <arpa/inet.h>
#include "ipaddress.h"

namespace swss {

class IpPrefix
{
public:
    IpPrefix() = default;
    IpPrefix(const std::string &ipPrefixStr);
    IpPrefix(uint32_t addr, int mask);
    IpPrefix(const ip_addr_t &ip, int mask);

    inline bool isV4() const
    {
        return m_ip.isV4();
    }

    inline IpAddress getIp() const
    {
        return m_ip;
    }

    inline IpAddress getBroadcastIp() const
    {
        switch (m_ip.getIp().family)
        {
            case AF_INET:
            {
                return IpAddress((m_ip.getV4Addr()) | ~(getMask().getV4Addr()));
            }
            case AF_INET6:
            {
                ip_addr_t ipa;
                ipa.family = AF_INET6;
                const uint8_t *prefix = m_ip.getV6Addr();
                IpAddress ip6mask = getMask();
                const uint8_t *mask = ip6mask.getV6Addr();

                for (int i = 0; i < 16; ++i)
                {
                    ipa.ip_addr.ipv6_addr[i] = (uint8_t)(prefix[i] | ~mask[i]);
                }
                return IpAddress(ipa);
            }
            default:
            {
                throw std::logic_error("Invalid family");
            }
        }
    }

    inline IpAddress getMask() const
    {
        switch (m_ip.getIp().family)
        {
            case AF_INET:
            {
                return IpAddress(htonl((uint32_t)((0xFFFFFFFFLL << (32 - m_mask)) & 0xFFFFFFFFLL)));
            }
            case AF_INET6:
            {
                ip_addr_t ipa;
                ipa.family = AF_INET6;

                assert(m_mask >= 0 && m_mask <= 128);
                int mid = m_mask >> 3;
                int bits = m_mask & 0x7;
                memset(ipa.ip_addr.ipv6_addr, 0xFF, mid);
                if (mid < 16)
                {
                    assert(mid >= 0 && mid < 16);
                    ipa.ip_addr.ipv6_addr[mid] = (uint8_t)(0xFF << (8 - bits));
                    if (mid < 15)
                        memset(ipa.ip_addr.ipv6_addr + mid + 1, 0, 16 - mid - 1);
                }
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

    inline bool isDefaultRoute() const
    {
        return (m_mask == 0);
    }

    inline bool isFullMask() const
    {
        if (m_ip.isV4())
            return (m_mask == 32);
        else
            return (m_mask == 128);
    }

    inline bool isAddressInSubnet(const IpAddress& addr) const
    {
        if (m_ip.getIp().family != addr.getIp().family)
        {
            return false;
        }

        switch (m_ip.getIp().family)
        {
            case AF_INET:
            {
                uint32_t mask = getMask().getV4Addr();
                return (m_ip.getV4Addr() & mask) == (addr.getV4Addr() & mask);
            }
            case AF_INET6:
            {
                const uint8_t *prefix = m_ip.getV6Addr();
                IpAddress ip6mask = getMask();
                const uint8_t *mask = ip6mask.getV6Addr();
                const uint8_t *ip = addr.getV6Addr();

                for (int i = 0; i < 16; ++i)
                {
                    if ((prefix[i] & mask[i]) != (ip[i] & mask[i]))
                    {
                        return false;
                    }
                }

                return true;
            }
            default:
            {
                throw std::logic_error("Invalid family");
            }
        }
    }

    inline IpPrefix getSubnet() const
    {
        switch (m_ip.getIp().family)
        {
            case AF_INET:
            {
                uint32_t ipaddr = m_ip.getV4Addr();
                uint32_t mask = getMask().getV4Addr();

                ipaddr &= mask;
                return IpPrefix(ipaddr, m_mask);
            }

            case AF_INET6:
            {
                const uint8_t *ipaddr = m_ip.getV6Addr();
                uint8_t subnet[INET6_ADDRSTRLEN] = {0};
                memcpy(subnet, ipaddr, 16);

                IpAddress ip6mask = getMask();
                const uint8_t *mask = ip6mask.getV6Addr();

                for (int i = 0; i < 16; ++i)
                {
                    subnet[i] &= mask[i];
                }

                char buf[INET6_ADDRSTRLEN];
                std::string ipStr(inet_ntop(AF_INET6,
                                            &subnet,
                                            buf,
                                            INET6_ADDRSTRLEN));

                return IpPrefix(ipStr + "/" + std::to_string(m_mask));
            }

            default:
            {
                throw std::logic_error("Invalid family");
            }
        }
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
    bool isValid();

    IpAddress m_ip;
    int m_mask;
};

}

#endif
