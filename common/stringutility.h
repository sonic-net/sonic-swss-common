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
static inline void lexical_convert(const std::string &str, T &t)
{
    t = boost::lexical_cast<T>(str);
}

namespace lexical_convert_detail
{

    template <typename T, typename... Args>
    void lexical_convert(
        std::vector<std::string>::const_iterator begin,
        std::vector<std::string>::const_iterator end,
        T &t)
    {
        if (begin == end)
        {
            SWSS_LOG_THROW("Insufficient corpus");
        }
        auto cur_itr = begin++;
        if (begin != end)
        {
            SWSS_LOG_THROW("Too much corpus");
        }
        swss::lexical_convert(*cur_itr, t);
    }

    template <typename T, typename... Args>
    void lexical_convert(
        std::vector<std::string>::const_iterator begin,
        std::vector<std::string>::const_iterator end,
        T &t,
        Args &... args)
    {
        if (begin == end)
        {
            SWSS_LOG_THROW("Insufficient corpus");
        }
        swss::lexical_convert(*(begin++), t);
        return lexical_convert(begin, end, args...);
    }

}

template <typename T, typename... Args>
void lexical_convert(const std::vector<std::string> &strs, T &t, Args &... args)
{
    lexical_convert_detail::lexical_convert(strs.begin(), strs.end(), t, args...);
}

namespace join_detail
{

    template <typename T>
    void join(std::ostringstream &ostream, char, const T &t)
    {
        ostream << t;
    }

    template <typename T, typename... Args>
    void join(std::ostringstream &ostream, char delimiter, const T &t, const Args &... args)
    {
        ostream << t << delimiter;
        join(ostream, delimiter, args...);
    }

    template <typename Iterator>
    void join(std::ostringstream &ostream, char delimiter, Iterator begin, Iterator end)
    {
        if (begin == end)
        {
            return;
        }

        ostream << *begin;
        while(++begin != end)
        {
            ostream << delimiter << *begin;
        }
    }
}

template <typename T, typename... Args>
static inline std::string join(char delimiter, const T &t, const Args &... args)
{
    std::ostringstream ostream;
    join_detail::join(ostream, delimiter, t, args...);
    return ostream.str();
}

template <typename Iterator>
static inline std::string join(char delimiter, Iterator begin, Iterator end)
{
    std::ostringstream ostream;
    join_detail::join(ostream, delimiter, begin, end);
    return ostream.str();
}

static inline bool hex_to_binary(const std::string &hex_str, std::uint8_t *buffer, size_t buffer_length)
{
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
static inline void hex_to_binary(const std::string &s, T &value)
{
    return hex_to_binary(s, &value, sizeof(T));
}

static inline std::string binary_to_hex(const void *buffer, size_t length)
{
    std::string s;
    auto buf = static_cast<const std::uint8_t *>(buffer);

    boost::algorithm::hex(
        buf,
        buf + length,
        std::back_inserter<std::string>(s));

    return s;
}

}
