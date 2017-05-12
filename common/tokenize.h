#ifndef __TOKENIZE__
#define __TOKENIZE__

#include <sstream>
#include <vector>

namespace swss {

std::vector<std::string> tokenize(const std::string &, const char token);
std::vector<std::string> tokenize(const std::string &, const char token, const size_t firstN);

}

#endif /* TOKENIZE */
