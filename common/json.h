#ifndef __JSON__
#define __JSON__

#include <string>
#include <vector>

#include "table.h"

namespace swss {

class JSon
{
public:
   static std::string buildJson(const std::vector<FieldValueTuple> &fv);
   static void readJson(const std::string &json, std::vector<FieldValueTuple> &fv);
};

}

#endif
