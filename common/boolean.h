#pragma once

#include <iostream>

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

std::ostream &operator<<(std::ostream &out, const Boolean &b);

std::istream &operator>>(std::istream &in, Boolean &b);

std::ostream &operator<<(std::ostream &out, const AlphaBoolean &b);

std::istream &operator>>(std::istream &in, AlphaBoolean &b);

}
