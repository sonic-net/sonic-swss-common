#include "stringutility.h"

#include <inttypes.h>
#include <algorithm>

std::istringstream &swss::operator>>(
    std::istringstream &istream,
    bool &b)
{
    std::string buffer = istream.str();
    std::transform(
        buffer.begin(),
        buffer.end(),
        buffer.begin(),
        ::tolower);
    if (buffer == "true" || buffer == "1")
    {
        b = true;
    }
    else if (buffer == "false" || buffer == "0")
    {
        b = false;
    }
    else
    {
        throw std::invalid_argument("Invalid bool string : " + buffer);
    }

    return istream;
}

bool swss::hex_to_binary(
    const std::string &hex_str,
    std::uint8_t *buffer,
    size_t buffer_length)
{
    SWSS_LOG_ENTER();

    if (hex_str.length() % 2 != 0)
    {
        SWSS_LOG_ERROR("Invalid hex string %s", hex_str.c_str());
        return false;
    }

    if (hex_str.length() > (buffer_length * 2))
    {
        SWSS_LOG_ERROR("Buffer length isn't sufficient");
        return false;
    }

    size_t buffer_cur = 0;
    size_t hex_cur = 0;
    unsigned char *output = static_cast<unsigned char *>(buffer);

    while (hex_cur < hex_str.length())
    {
        const char temp_buffer[] = { hex_str[hex_cur], hex_str[hex_cur + 1], 0 };
        unsigned int value = -1;

        if (sscanf(temp_buffer, "%X", &value) <= 0 || value > 0xff)
        {
            SWSS_LOG_ERROR("Invalid hex string %s", temp_buffer);
            return false;
        }

        output[buffer_cur] = static_cast<unsigned char>(value);
        hex_cur += 2;
        buffer_cur += 1;
    }

    return true;
}

std::string swss::binary_to_hex(const void *buffer, size_t length)
{
    SWSS_LOG_ENTER();

    std::string s;

    if (buffer == NULL || length == 0)
    {
        return s;
    }

    s.resize(2 * length, '0');

    const unsigned char *input = static_cast<const unsigned char *>(buffer);
    char *output = &s[0];

    for (size_t i = 0; i < length; i++)
    {
        snprintf(&output[i * 2], 3, "%02X", input[i]);
    }

    return s;
}

