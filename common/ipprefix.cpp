#include <arpa/inet.h>
#include <string>
#include <stdexcept>

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
        try
        {
            m_mask = std::stoi(ipPrefixStr.substr(pos + 1));
        }
        catch(const std::logic_error& ex)
        {
            throw std::invalid_argument(std::string("Failed to covert mask: ") + ex.what());
        }
        
        if (!isValid())
        {
            throw std::invalid_argument("Invalid IpPrefix from string");
        }
    }
}

IpPrefix::IpPrefix(uint32_t ipPrefix, int mask)
{
    m_ip = IpAddress(ipPrefix);
    m_mask = mask;
    if (!isValid())
    {
        throw std::invalid_argument("Invalid IpPrefix from prefix and mask");
    }
}

bool IpPrefix::isValid()
{
    if (m_mask < 0) return false;
    
    switch (m_ip.getIp().family)
    {
        case AF_INET:
        {
            if (m_mask > 32) return false;
            break;
        }
        case AF_INET6:
        {
            if (m_mask > 128) return false;
            break;
        }
        default:
        {
            return false;
        }
    }
    return true;
}

std::string IpPrefix::to_string() const
{
    return (m_ip.to_string() + "/" + std::to_string(m_mask));
}
