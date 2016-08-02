#include "tokenize.h"

using namespace std;

namespace swss {

vector<string> tokenize(const string &str, const char token)
{
    string tmp;
    vector<string> ret;
    istringstream iss(str);

    while (getline(iss, tmp, token))
        ret.push_back(tmp);

    return ret;
}

}