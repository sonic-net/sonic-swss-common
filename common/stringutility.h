#pragma once

#include "logger.h"
#include "tokenize.h"
#include "schema.h"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/hex.hpp>

#include <inttypes.h>
#include <algorithm>
#include <iterator>
#include <vector>
#include <string>
#include <sstream>
#include <cctype>

namespace swss {

template<typename T>
void cast(const std::string &str, T &t)
{
    SWSS_LOG_ENTER();
    t = boost::lexical_cast<T>(str);
}

static inline void cast(const std::string &str, bool &b)
{
    SWSS_LOG_ENTER();
    try
    {
        b = boost::lexical_cast<bool>(str);
    }
    catch(const boost::bad_lexical_cast& e)
    {
        if (str == TRUE_STRING)
        {
            b = true;
        }
        else if (str == FALSE_STRING)
        {
            b = false;
        }
        else
        {
            throw e;
        }
    }
}

template <typename T, typename... Args>
void cast(
    std::vector<std::string>::const_iterator begin,
    std::vector<std::string>::const_iterator end,
    T &t)
{
    SWSS_LOG_ENTER();
    if (begin == end)
    {
        SWSS_LOG_THROW("Insufficient corpus");
    }
    auto cur_itr = begin++;
    if (begin != end)
    {
        SWSS_LOG_THROW("Too much corpus");
    }
    cast(*cur_itr, t);
}

template <typename T, typename... Args>
void cast(
    std::vector<std::string>::const_iterator begin,
    std::vector<std::string>::const_iterator end,
    T &t,
    Args &... args)
{
    SWSS_LOG_ENTER();
    if (begin == end)
    {
        SWSS_LOG_THROW("Insufficient corpus");
    }
    cast(*(begin++), t);
    return cast(begin, end, args...);
}

template <typename T, typename... Args>
void cast(const std::vector<std::string> &strs, T &t, Args &... args)
{
    cast(strs.begin(), strs.end(), t, args...);
}

template <typename T>
void join(std::ostringstream &ostream, char, const T &t)
{
    SWSS_LOG_ENTER();
    ostream << t;
}

template <typename T, typename... Args>
void join(std::ostringstream &ostream, char delimiter, const T &t, const Args &... args)
{
    SWSS_LOG_ENTER();
    ostream << t << delimiter;
    join(ostream, delimiter, args...);
}

template <typename T, typename... Args>
std::string join(char delimiter, const T &t, const Args &... args)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    join(ostream, delimiter, t, args...);
    return ostream.str();
}

static inline bool hex_to_binary(const std::string &hex_str, std::uint8_t *buffer, size_t buffer_length)
{
    SWSS_LOG_ENTER();

    if (hex_str.length() != (buffer_length * 2))
    {
        SWSS_LOG_DEBUG("Buffer length isn't sufficient");
        return false;
    }

    try
    {
        boost::algorithm::unhex(hex_str, buffer);
    }
    catch(const boost::algorithm::non_hex_input &e)
    {
        SWSS_LOG_DEBUG("Invalid hex string %s", hex_str.c_str());
        return false;
    }

    return true;
}

template<typename T>
void hex_to_binary(
    const std::string &s,
    T &value)
{
    SWSS_LOG_ENTER();

    return hex_to_binary(s, &value, sizeof(T));
}

static inline std::string binary_to_hex(const void *buffer, size_t length)
{
    SWSS_LOG_ENTER();

    std::string s;
    auto buf = static_cast<const std::uint8_t *>(buffer);

    boost::algorithm::hex(
        buf,
        buf + length,
        std::back_inserter<std::string>(s));

    return s;
}

}
