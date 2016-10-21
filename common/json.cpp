#include <sstream>
#include <limits>

#include "common/json.h"
#include "common/json.hpp"

using namespace std;

namespace swss {

string JSon::buildJson(const vector<FieldValueTuple> &fv)
{
    nlohmann::json j;

    for (auto &i : fv)
    {
        j[fvField(i)] = fvValue(i);
    }

    return j.dump();
}

void JSon::readJson(const string &jsonstr, vector<FieldValueTuple> &fv)
{
    nlohmann::json j = nlohmann::json::parse(jsonstr);

    FieldValueTuple e;

    std::map<std::string, nlohmann::json> m = j;

    for (auto&& kv : m)
    {
        fvField(e) = kv.first;
        fvValue(e) = kv.second;
        fv.push_back(e);
    }
}

}
