#include <stdexcept>

#include "macaddress.h"

using namespace swss;
using namespace std;

const size_t mac_address_str_length = ETHER_ADDR_LEN*2 + 5; // 6 hexadecimal numbers (two digits each) + 5 delimiters

MacAddress::MacAddress()
{
    memset(m_mac, 0, ETHER_ADDR_LEN);
}

MacAddress::MacAddress(const uint8_t *mac)
{
    memcpy(m_mac, mac, ETHER_ADDR_LEN);
}

MacAddress::MacAddress(const std::string& macStr)
{
    bool suc = MacAddress::parseMacString(macStr, m_mac);
    if (!suc) throw invalid_argument("can't parse mac address '" + macStr + "'");
}

const std::string MacAddress::to_string() const
{
    return MacAddress::to_string(m_mac);
}

std::string MacAddress::to_string(const uint8_t* mac)
{
    const static char char_table[] = "0123456789abcdef";

    std::string str(mac_address_str_length, ':');
    for(int i = 0; i < ETHER_ADDR_LEN; ++i) {
        int left = i * 3;      // left  digit position of hexadecimal number
        int right = left + 1;  // right digit position of hexadecimal number
        int left_half  = mac[i] >> 4;
        int right_half = mac[i] & 0x0f;

        str[left]  = char_table[left_half];
        str[right] = char_table[right_half];
    }

    return str;
}

// This function parses a string to a binary mac address (uint8_t[6])
// The string should contain mac address only. No spaces are allowed.
// The mac address separators could be either ':' or '-'
bool MacAddress::parseMacString(const string& str_mac, uint8_t* bin_mac)
{
    if (bin_mac == NULL)
    {
        return false;
    }

    if (str_mac.length() != mac_address_str_length)
    {
        return false;
    }

    const char* ptr_mac = str_mac.c_str();

    // first check that all mac address separators are equal to each other
    // 2, 5, 8, 11, and 14 are MAC address separator positions
    if (!(ptr_mac[2]  == ptr_mac[5]
       && ptr_mac[5]  == ptr_mac[8]
       && ptr_mac[8]  == ptr_mac[11]
       && ptr_mac[11] == ptr_mac[14]))
    {
        return false;
    }

    // then check that the first separator is equal to ':' or '-'
    if (ptr_mac[2] != ':' && ptr_mac[2] != '-')
    {
        return false;
    }

    for(int i = 0; i < ETHER_ADDR_LEN; ++i)
    {
        int left  = i * 3;    // left  digit position of hexadecimal number
        int right = left + 1; // right digit position of hexadecimal number

        if (ptr_mac[left] >= '0' && ptr_mac[left] <= '9')
        {
            bin_mac[i] = static_cast<uint8_t>(ptr_mac[left] - '0');
        }
        else if (ptr_mac[left] >= 'A' && ptr_mac[left] <= 'F')
        {
            bin_mac[i] = static_cast<uint8_t>(ptr_mac[left] - 'A' + 0x0a);
        }
        else if (ptr_mac[left] >= 'a' && ptr_mac[left] <= 'f')
        {
            bin_mac[i] = static_cast<uint8_t>(ptr_mac[left] - 'a' + 0x0a);
        }
        else
        {
            return false;
        }

        bin_mac[i] = static_cast<uint8_t>(bin_mac[i] << 4);

        if (ptr_mac[right] >= '0' && ptr_mac[right] <= '9')
        {
            bin_mac[i] |= static_cast<uint8_t>(ptr_mac[right] - '0');
        }
        else if (ptr_mac[right] >= 'A' && ptr_mac[right] <= 'F')
        {
            bin_mac[i] |= static_cast<uint8_t>(ptr_mac[right] - 'A' + 0x0a);
        }
        else if (ptr_mac[right] >= 'a' && ptr_mac[right] <= 'f')
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
