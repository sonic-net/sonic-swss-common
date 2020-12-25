#pragma once

#include <iostream>

namespace swss
{
class Boolean
{
public:
    operator bool() const;
protected:
    Boolean() = default;
    Boolean(bool boolean);
    bool m_boolean;
};

class AlphaBoolean : public Boolean
{
public:
    static const std::string ALPHA_TRUE;
    static const std::string ALPHA_FALSE;
    AlphaBoolean() = default;
    AlphaBoolean(bool boolean);
};

std::ostream &operator<<(std::ostream &out, const AlphaBoolean &b);

std::istream &operator>>(std::istream &in, AlphaBoolean &b);

}
