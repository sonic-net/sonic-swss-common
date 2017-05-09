#ifndef __MACADDRESS__
#define __MACADDRESS__

#include <string.h>
#include <stdint.h>
#include <string>

namespace swss {

class MacAddress
{
public:

    MacAddress();

    MacAddress(const uint8_t *mac);

    MacAddress(const std::string& macStr);

    inline void getMac(uint8_t *mac) const
    {
        memcpy(mac, m_mac, 6);
    }

    inline const uint8_t *getMac() const
    {
        return m_mac;
    }

    inline bool operator==(const MacAddress &o) const
    {
        return !memcmp(m_mac, o.m_mac, 6);
    }

    inline bool operator!=(const MacAddress &o) const
    {
        return !(*this == o);
    }

    inline bool operator<(const MacAddress &o) const
    {
        return memcmp(m_mac, o.m_mac, sizeof(m_mac)) < 0;
    }

    inline bool operator!() const
    {
        return (!m_mac[0] && !m_mac[1] && !m_mac[2] &&
                !m_mac[3] && !m_mac[4] && !m_mac[5]);
    }

    inline operator bool() const
    {
        return !!(*this);
    }

    const std::string to_string() const;

    static std::string to_string(const uint8_t* mac);

    static bool parseMacString(const std::string& strmac, uint8_t* mac);

private:
    uint8_t m_mac[6];
};

}

#endif
