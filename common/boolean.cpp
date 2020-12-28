#include "boolean.h"

#include <vector>
#include <ios>

std::ostream &swss::operator<<(std::ostream &out, const swss::AlphaBoolean &b)
{
    bool value = b;
    out << std::boolalpha << value;
    return out;
}

std::istream &swss::operator>>(std::istream &in, swss::AlphaBoolean &b)
{
    bool value = false;
    in >> std::boolalpha >> value;
    b = value;
    return in;
}

std::ostream &swss::operator<<(std::ostream &out, const swss::Boolean &b)
{
    bool value = b;
    out << value;
    return out;
}

std::istream &swss::operator>>(std::istream &in, swss::Boolean &b)
{
    bool value = false;
    in >> value;
    b = value;
    return in;
}
