#pragma once

#include <iostream>
#include <ios>

namespace swss
{
class Boolean
{
public:
    Boolean() = default;
    Boolean(bool boolean) : m_boolean(boolean)
    {
    }
    operator bool() const
    {
        return m_boolean;
    }
protected:
    bool m_boolean;
};

class AlphaBoolean : public Boolean
{
public:
    AlphaBoolean() = default;
    AlphaBoolean(bool boolean) : Boolean(boolean)
    {
    }
};

static inline std::ostream &operator<<(std::ostream &out, const AlphaBoolean &b)
{
    bool value = b;
    out << std::boolalpha << value;
    return out;
}

static inline std::istream &operator>>(std::istream &in, AlphaBoolean &b)
{
    bool value = false;
    in >> std::boolalpha >> value;
    b = value;
    return in;
}

static inline std::ostream &operator<<(std::ostream &out, const Boolean &b)
{
    bool value = b;
    out << value;
    return out;
}

static inline std::istream &operator>>(std::istream &in, Boolean &b)
{
    bool value = false;
    in >> value;
    b = value;
    return in;
}

}
