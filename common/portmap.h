#ifndef SWSS_COMMON_PORTMAP_H
#define SWSS_COMMON_PORTMAP_H

#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace swss {

std::map<std::set<int>, std::string> handlePortMap(std::string file);

}

#endif /* SWSS_COMMON_PORTMAP_H */
