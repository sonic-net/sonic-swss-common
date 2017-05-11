#include "macaddress.h"

#include <sstream>
#include <stdexcept>

using namespace swss;
using namespace std;

MacAddress::MacAddress()
{
    memset(m_mac, 0, 6);
}

MacAddress::MacAddress(const uint8_t *mac)
{
    memcpy(m_mac, mac, 6);
}

MacAddress::MacAddress(const std::string& macStr)
{
    bool suc = MacAddress::parseMacString(macStr, m_mac);
    if (!suc) throw invalid_argument("macStr");
}

const std::string MacAddress::to_string() const
{
    char tmp_mac[32];
    sprintf(tmp_mac, "%02x:%02x:%02x:%02x:%02x:%02x", m_mac[0], m_mac[1], m_mac[2], m_mac[3], m_mac[4], m_mac[5]);
    return std::string(tmp_mac);
}

std::string MacAddress::to_string(const uint8_t* mac)
{
    uint8_t tmp_mac[32];
    sprintf((char*)tmp_mac, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string((char*)tmp_mac);
}

bool MacAddress::parseMacString(const string& macStr, uint8_t* mac)
{
    int i;
    unsigned int value;
    char ignore;
    istringstream iss(macStr);

    if (mac == NULL)
    {
        return false;
    }

    iss >> hex;

    for (i = 0; i < 5; i++)
    {
        iss >> value >> ignore;
        if (!iss) return false;
        if (value >= 256) return false;
        mac[i] = (uint8_t)value;
    }
    iss >> value;
    mac[5] = (uint8_t)value;

    return true;
}
