#pragma once

#include "logger.h"
#include "tokenize.h"

#include <vector>
#include <string>
#include <sstream>
#include <cctype>

namespace swss {

template <typename T>
bool split(const std::string &input, char delimiter, T &output)
{
    SWSS_LOG_ENTER();
    if (input.find(delimiter) != std::string::npos)
    {
        return false;
    }
    std::istringstream istream(input);
    istream >> output;
    return true;
}

template <typename T, typename... Args>
bool split(
    std::vector<std::string>::iterator begin_itr,
    std::vector<std::string>::iterator end_itr,
    T &output)
{
    SWSS_LOG_ENTER();
    auto cur_itr = begin_itr++;
    if (begin_itr != end_itr)
    {
        return false;
    }
    std::istringstream istream(*cur_itr);
    istream >> output;
    return true;
}

template <typename T, typename... Args>
bool split(
    std::vector<std::string>::iterator begin_itr,
    std::vector<std::string>::iterator end_itr,
    T &output,
    Args &... args)
{
    SWSS_LOG_ENTER();
    if (begin_itr == end_itr)
    {
        return false;
    }
    std::istringstream istream(*(begin_itr++));
    istream >> output;
    return split(begin_itr, end_itr, args...);
}

template <typename T, typename... Args>
bool split(const std::string &input, char delimiter, T &output, Args &... args)
{
    SWSS_LOG_ENTER();
    auto tokens = tokenize(input, delimiter);
    return split(tokens.begin(), tokens.end(), output, args...);
}

template <typename T>
std::string join(__attribute__((unused)) char delimiter, const T &input)
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
