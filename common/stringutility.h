#pragma once

#include "logger.h"
#include "tokenize.h"

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cctype>
#include <type_traits>

namespace swss {

template<typename T>
T lexical_cast(const std::string &str)
{
    return boost::lexical_cast<T>(str);
}

template<>
bool lexical_cast<bool>(const std::string &str);

template <typename T>
bool split(const std::string &input, char delimiter, T &output)
{
    SWSS_LOG_ENTER();
    if (input.find(delimiter) != std::string::npos)
    {
        return false;
    }
    output = lexical_cast<T>(input);
    return true;
}

template <typename T, typename... Args>
bool split(
    std::vector<std::string>::iterator begin,
    std::vector<std::string>::iterator end,
    T &output)
{
    SWSS_LOG_ENTER();
    auto cur_itr = begin++;
    if (begin != end)
    {
        return false;
    }
    output = lexical_cast<T>(*cur_itr);
    return true;
}

template <typename T, typename... Args>
bool split(
    std::vector<std::string>::iterator begin,
    std::vector<std::string>::iterator end,
    T &output,
    Args &... args)
{
    SWSS_LOG_ENTER();
    if (begin == end)
    {
        return false;
    }
    output = lexical_cast<T>(*(begin++));
    return split(begin, end, args...);
}

template <typename T, typename... Args>
bool split(const std::string &input, char delimiter, T &output, Args &... args)
{
    SWSS_LOG_ENTER();
    auto tokens = tokenize(input, delimiter);
    return split(tokens.begin(), tokens.end(), output, args...);
}

template <typename T>
std::string join(char , const T &input)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream << input;
    return ostream.str();
}

template <typename T, typename... Args>
std::string join(char delimiter, const T &input, const Args &... args)
{
    SWSS_LOG_ENTER();
    std::ostringstream ostream;
    ostream << input << delimiter << join(delimiter, args...);
    return ostream.str();
}

std::istringstream &operator>>(std::istringstream &istream, bool &b);

bool hex_to_binary(const std::string &hex_str, std::uint8_t *buffer, size_t buffer_length);

template<typename T>
void hex_to_binary(
    const std::string &s,
    T &value)
{
    SWSS_LOG_ENTER();

    return hex_to_binary(s, &value, sizeof(T));
}

std::string binary_to_hex(const void *buffer, size_t length);

}
