#include <arpa/inet.h>
#include <string>

#include "ipprefix.h"

using namespace swss;

IpPrefix::IpPrefix(
    const std::string &ipPrefixStr)
{
    size_t pos = ipPrefixStr.find('/');
    std::string ipStr = ipPrefixStr.substr(0, pos);
    if (ipStr.empty())
    {
        m_ip = IpAddress(0);
    }
    else
    {
        m_ip = IpAddress(ipStr);
    }

    if (pos == std::string::npos)
    {
        if (m_ip.isV4())
            m_mask = 32;
        else
            m_mask = 128;
    }
    else
    {
        m_mask = std::stoi(ipPrefixStr.substr(pos + 1));
    }
}

IpPrefix::IpPrefix(uint32_t ipPrefix, int mask)
{
    m_ip = IpAddress(ipPrefix);
    m_mask = mask;
}

const std::string IpPrefix::to_string() const
{
    return (m_ip.to_string() + "/" + std::to_string(m_mask));
}
