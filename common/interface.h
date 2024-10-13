#ifndef __INTERFACE__
#define __INTERFACE__

#include <string>
#include <net/if.h>

namespace swss {

bool isInterfaceNameLenOk(const std::string &ifaceName)
{
    if (ifaceName.empty() || ifaceName.length() >= IFNAMSIZ)
    {
        SWSS_LOG_ERROR("Invalid interface name %s length, it must not exceed %d characters", ifaceName.c_str(), IFNAMSIZ);
        return false;
    }
    return true;
}

size_t ifaceNameMaxLen()
{
    return IFNAMSIZ;
}

#if defined(SWIG) && defined(SWIGPYTHON)
%pythoncode %{
    def validate_interface_name_length(iface_name):
        return isInterfaceNameLenOk(iface_name)

    def iface_name_max_length():
        return ifaceNameMaxLen()
%}
#endif

}

#endif
