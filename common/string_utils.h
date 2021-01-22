#ifndef __STRING_UTILS__
#define __STRING_UTILS__

#include <algorithm>
#include <sstream>
#include <string>

namespace swss {

void toupper(std::string &str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){return std::toupper(c);});
}

void tolower(std::string &str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){return std::tolower(c);});
}

template<typename Items, typename Delim>
std::string join(const Items &items, const Delim &delim)
{
    if (items.empty())
    {
        return std::string();
    }

    std::ostringstream oss;
    auto iter = std::begin(items);
    auto end = std::end(items);
    oss << *iter;
    while(++iter != end)
    {
        oss << delim << *iter;
    }

    return oss.str();
}

}

#endif /* __STRING_UTILS__ */
