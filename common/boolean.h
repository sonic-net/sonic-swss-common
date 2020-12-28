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
    operator bool&()
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
    return out << std::boolalpha << (bool)(b);
}

static inline std::istream &operator>>(std::istream &in, AlphaBoolean &b)
{
    return in >> std::boolalpha >> (bool &)(b);
}

static inline std::ostream &operator<<(std::ostream &out, const Boolean &b)
{
    return out << (bool)(b);
}

static inline std::istream &operator>>(std::istream &in, Boolean &b)
{
    return in >> (bool &)(b);
}

}
