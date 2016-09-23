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
    if (ipStr.find(":") != std::string::npos)
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
