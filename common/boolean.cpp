#include "boolean.h"

#include <vector>

swss::Boolean::operator bool() const
{
    return m_boolean;
}

swss::Boolean::Boolean(bool boolean) : m_boolean(boolean)
{
}

swss::AlphaBoolean::AlphaBoolean(bool boolean) : Boolean(boolean)
{
}

const std::string swss::AlphaBoolean::ALPHA_TRUE = "true";
const std::string swss::AlphaBoolean::ALPHA_FALSE = "false";

std::ostream &swss::operator<<(std::ostream &out, const swss::AlphaBoolean &b)
{
    out << (b ? swss::AlphaBoolean::ALPHA_TRUE : swss::AlphaBoolean::ALPHA_FALSE);
    return out;
}

std::istream &swss::operator>>(std::istream &in, swss::AlphaBoolean &b)
{
    bool target_bool = false;
    std::string target_string;
    std::string received_string;
    if (in.peek() == 't')
    {
        target_string = swss::AlphaBoolean::ALPHA_TRUE;
        target_bool = true;
    }
    else if (in.peek() == 'f')
    {
        target_string = swss::AlphaBoolean::ALPHA_FALSE;
        target_bool = false;
    }
    else
    {
        in.setstate(std::istream::failbit);
        return in;
    }

    received_string.resize(target_string.length(), 0);
    in.read(&received_string[0], received_string.length());

    if (target_string != received_string)
    {
        for (auto itr = received_string.begin(); itr != received_string.end(); ++itr)
        {
            if (*itr != 0)
            {
                in.putback(*itr);
            }
        }
        in.setstate(std::istream::failbit);
    }
    else
    {
        b = target_bool;
    }

    return in;
}
