#ifndef __INTERFACE__
#define __INTERFACE__

#include <string>
#include <net/if.h>

namespace swss
{

const size_t IFACE_NAME_MAX_LEN = IFNAMSIZ - 1;

inline bool isInterfaceNameValid(const std::string &ifaceName)
{
    if (ifaceName.empty() || ifaceName.length() >= IFNAMSIZ || ifaceName == "." || ifaceName == "..")
    {
        return false;
    }

    for (size_t index = 0; index < ifaceName.size(); ++index)
    {
        const unsigned char character = static_cast<unsigned char>(ifaceName[index]);
        const bool isAlphaNumeric =
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9');
        const bool isCommonCharacter =
            isAlphaNumeric || character == '_' || character == '.';

        if (isCommonCharacter || (index > 0 && character == '-'))
        {
            continue;
        }

        return false;
    }

    return true;
}

}

#endif
