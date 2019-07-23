#include <arpa/inet.h>
#include <stdexcept>

#include "ipaddress.h"

using namespace swss;

IpAddress::IpAddress(uint32_t ip)
{
    m_ip.family = AF_INET;
    m_ip.ip_addr.ipv4_addr = ip;
}

IpAddress::IpAddress(const std::string &ipStr)
{
    if (ipStr.find(':') != std::string::npos)
    {
        m_ip.family = AF_INET6;
    }
    else
    {
        m_ip.family = AF_INET;
    }

    if (inet_pton(m_ip.family, ipStr.c_str(), &m_ip.ip_addr) != 1)
    {
        std::string err = "Error converting " + ipStr + " to IP address";
        throw std::invalid_argument(err);
    }
}

std::string IpAddress::to_string() const
{
    char buf[INET6_ADDRSTRLEN];

    std::string ipStr(inet_ntop(m_ip.family, &m_ip.ip_addr, buf, INET6_ADDRSTRLEN));

    return ipStr;
}

IpAddress::AddrScope IpAddress::getAddrScope() const
{
    /* Auxiliary prefixes used to determine the scope of any given address */
    static const IpAddress ipv4LinkScopeAddress = IpAddress("169.254.0.0");
    static const IpAddress ipv6LinkScopeAddress = IpAddress("FE80::0");
    static const IpAddress ipv4HostScopeAddress = IpAddress("127.0.0.1");
    static const IpAddress ipv6HostScopeAddress = IpAddress("::1");
    static const IpAddress ipv4McastScopeAddress = IpAddress("224.0.0.0");
    static const IpAddress ipv6McastScopeAddress = IpAddress("FF00::0");

    if (isV4())
    {
        const uint32_t ip1 = htonl(getV4Addr());
        const uint32_t ip2 = htonl(ipv4LinkScopeAddress.getV4Addr());
        const uint32_t ip3 = htonl(ipv4McastScopeAddress.getV4Addr());

        /* IPv4 local-scope mask is 16 bits long -- mask = 0xffff0000 */
        if ((ip1 & 0xffff0000) == ip2)
        {
            return LINK_SCOPE;
        }
        else if (*this == ipv4HostScopeAddress)
        {
            return HOST_SCOPE;
        }
        /* IPv4 multicast-scope mask is 4 bits long -- mask = 0xf0000000 */
        else if ((ip1 & 0xf0000000) == ip3)
        {
            return MCAST_SCOPE;
        }
    }
    else
    {
        const uint8_t *ip1 = getV6Addr();
        const uint8_t *ip2 = ipv6LinkScopeAddress.getV6Addr();
        const uint8_t *ip3 = ipv6McastScopeAddress.getV6Addr();

        /* IPv6 local-scope mask is 10 bits long -- mask = 0xffc0::0 */
        if ((ip1[0] == ip2[0]) && ((ip1[1] & 0xc0) == ip2[1]))
        {
            return LINK_SCOPE;
        }
        else if (*this == ipv6HostScopeAddress)
        {
            return HOST_SCOPE;
        }
        /* IPv6 multicast-scope mask is 8 bits long -- mask = 0xff00::0 */
        else if (ip1[0] == ip3[0])
        {
            return MCAST_SCOPE;
        }
    }

    return GLOBAL_SCOPE;
}
