#include "stringutility.h"

#include <boost/algorithm/hex.hpp>

#include <inttypes.h>
#include <algorithm>
#include <iterator>

std::istringstream &swss::operator>>(
    std::istringstream &istream,
    bool &b)
{
    SWSS_LOG_ENTER();

    bool valid_bool_string = true;
    char c;
    istream >> c;
    if (std::isdigit(c))
    {
        if (c == '1')
        {
            b = true;
        }
        else if (c == '0')
        {
            b = false;
        }
        else
        {
            valid_bool_string = false;
        }
    }
    else
    {
        std::string expect_string;
        if (std::tolower(c) == 't')
        {
            expect_string = "true";
            b = true;
        }
        else if (std::tolower(c) == 'f')
        {
            expect_string = "false";
            b = false;
        }
        if (expect_string.empty())
        {
            valid_bool_string = false;
        }
        else
        {
            std::string receive_string(expect_string.length(), 0);
            receive_string[0] = static_cast<char>(std::tolower(c));
            istream.read(&receive_string[1], expect_string.length() - 1);
            std::transform(
                receive_string.begin(),
                receive_string.end(),
                receive_string.begin(),
                ::tolower);
            valid_bool_string = (receive_string == expect_string);
        }
    }
    if (!valid_bool_string)
    {
        throw std::invalid_argument("Invalid bool string : " + istream.str());
    }
    return istream;
}

bool swss::hex_to_binary(
    const std::string &hex_str,
    std::uint8_t *buffer,
    size_t buffer_length)
{
    SWSS_LOG_ENTER();

    if (hex_str.length() > (buffer_length * 2))
    {
        SWSS_LOG_ERROR("Buffer length isn't sufficient");
        return false;
    }

    try
    {
        boost::algorithm::unhex(hex_str, buffer);
        return true;
    }
    catch(const boost::algorithm::not_enough_input &e)
    {
        SWSS_LOG_ERROR("Not enough input %s", hex_str.c_str());
    }
    catch(const boost::algorithm::non_hex_input &e)
    {
        SWSS_LOG_ERROR("Invalid hex string %s", hex_str.c_str());
    }

    return false;
}

std::string swss::binary_to_hex(const void *buffer, size_t length)
{
    SWSS_LOG_ENTER();

    std::string s;
    auto _buffer = static_cast<const std::uint8_t *>(buffer);

    boost::algorithm::hex(
        _buffer,
        _buffer + length,
        std::insert_iterator<std::string>(s, s.end()));

    return s;
}
