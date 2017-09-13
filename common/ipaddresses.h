#ifndef __IPADDRESSES__
#define __IPADDRESSES__

#include <set>

#include "ipaddress.h"

namespace swss {

class IpAddresses
{
public:
    IpAddresses() {}

    /* A list of IpAddresses separated by ',' */
    IpAddresses(const std::string &ips);

    inline const std::set<IpAddress> &getIpAddresses() const
    {
        return m_ips;
    }

    inline size_t getSize() const
    {
        return m_ips.size();
    }

    inline bool operator<(const IpAddresses &o) const
    {
        return m_ips < o.m_ips;
    }

    inline bool operator==(const IpAddresses &o) const
    {
        return m_ips == o.m_ips;
    }

    inline bool operator!=(const IpAddresses &o) const
    {
        return !(*this == o);
    }

    void add(const std::string &ip);

    void add(const IpAddress &ip);

    bool contains(const std::string &ip) const;

    bool contains(const IpAddress &ip) const;

    bool contains(const IpAddresses &ips) const;

    void remove(const std::string &ip);

    void remove(const IpAddress &ip);

    const std::string to_string() const;


private:
    std::set<IpAddress> m_ips;
};

}

#endif
