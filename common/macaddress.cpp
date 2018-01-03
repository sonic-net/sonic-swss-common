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
    return MacAddress::to_string(m_mac);
}

std::string MacAddress::to_string(const uint8_t* mac)
{
    std::string str(17, ':');
    for(int i = 0; i < 6; i++) {
        int left = i * 3;
        int right = left + 1;
        char left_half = static_cast<char>(mac[i] >> 4);
        char right_half = mac[i] & 0x0f;

        if (left_half >= 0 && left_half <= 9)
        {
            str[left] = static_cast<char>(left_half + '0');
        } else {
            str[left] = static_cast<char>(left_half + 'a' - 0x0a);
        }

        if (right_half >= 0 && right_half <= 9)
        {
            str[right] = static_cast<char>(right_half + '0');
        } else {
            str[right] = static_cast<char>(right_half + 'a' - 0x0a);
        }
    }

    return str;
}

bool MacAddress::parseMacString(const string& str_mac, uint8_t* bin_mac)
{
    if (bin_mac == NULL)
    {
        return false;
    }

    if (str_mac.length() != 17)
    {
        return false;
    }

    const char* ptr_mac = str_mac.c_str();

    if ((ptr_mac[2]  != ':' || ptr_mac[5]  != ':' || ptr_mac[8] != ':' || ptr_mac[11] != ':' || ptr_mac[14] != ':')
     && (ptr_mac[2]  != '-' || ptr_mac[5]  != '-' || ptr_mac[8] != '-' || ptr_mac[11] != '-' || ptr_mac[14] != '-'))
    {
        return false;
    }

    for(int i = 0; i < 6; i++)
    {
        int left  = i * 3;
        int right = left + 1;

        if (ptr_mac[left] >= '0' &&  ptr_mac[left] <= '9')
        {
            bin_mac[i] = static_cast<uint8_t>(ptr_mac[left] - '0');
        }
        else if (ptr_mac[left] >= 'A' &&  ptr_mac[left] <= 'F')
        {
            bin_mac[i] = static_cast<uint8_t>(ptr_mac[left] - 'A' + 0x0a);
        }
        else if (ptr_mac[left] >= 'a' &&  ptr_mac[left] <= 'f')
        {
            bin_mac[i] = static_cast<uint8_t>(ptr_mac[left] - 'a' + 0x0a);
        }
        else
        {
            return false;
        }

        bin_mac[i] = static_cast<uint8_t>(bin_mac[i] << 4);

        if (ptr_mac[right] >= '0' &&  ptr_mac[right] <= '9')
        {
            bin_mac[i] |= static_cast<uint8_t>(ptr_mac[right] - '0');
        }
        else if (ptr_mac[right] >= 'A' &&  ptr_mac[right] <= 'F')
        {
            bin_mac[i] |= static_cast<uint8_t>(ptr_mac[right] - 'A' + 0x0a);
        }
        else if (ptr_mac[right] >= 'a' &&  ptr_mac[right] <= 'f')
        {
            bin_mac[i] |= static_cast<uint8_t>(ptr_mac[right] - 'a' + 0x0a);
        }
        else
        {
            return false;
        }
    }

    return true;
}
