#ifndef __INTERFACE__
#define __INTERFACE__

#include <string>
#include <net/if.h>

namespace swss
{

const size_t IFACE_NAME_MAX_LEN = IFNAMSIZ - 1;

bool isInterfaceNameValid(const std::string &ifaceName)
{
    return !ifaceName.empty() && (ifaceName.length() < IFNAMSIZ);
}

}

#endif
