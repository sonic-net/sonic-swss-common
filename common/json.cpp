#include <sstream>
#include <limits>

#include "common/json.h"
#include "common/json.hpp"

using namespace std;

namespace swss {

string JSon::buildJson(const vector<FieldValueTuple> &fv)
{
    nlohmann::json j = nlohmann::json::array();

    // we use array to save order
    for (auto &i : fv)
    {
        j.push_back(fvField(i));
        j.push_back(fvValue(i));
    }

    return j.dump();
}

void JSon::readJson(const string &jsonstr, vector<FieldValueTuple> &fv)
{
    nlohmann::json j = nlohmann::json::parse(jsonstr);

    FieldValueTuple e;

    for (size_t i = 0; i < j.size(); i+=2)
    {
        fvField(e) = j[i];
        fvValue(e) = j[i+1];
        fv.push_back(e);
    }
}

}
