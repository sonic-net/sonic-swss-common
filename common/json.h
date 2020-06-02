#ifndef __JSON__
#define __JSON__

#include <string>
#include <fstream>
#include <vector>

#include "table.h"

namespace swss {

class JSon
{
private:
    static const int el_count = 2;
public:
    static std::string buildJson(const std::vector<FieldValueTuple> &fv);
    static void readJson(const std::string &json, std::vector<FieldValueTuple> &fv);
    static bool loadJsonFromFile(std::ifstream &fs, std::vector<KeyOpFieldsValuesTuple> &db_items);
};

}

#endif
