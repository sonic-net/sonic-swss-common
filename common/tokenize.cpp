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
vector<string> tokenize(const string &str, const char token, const size_t firstN)
{
    vector<string> ret;
    string tmp = str;
    size_t i = 0;
    auto pos = tmp.find(token);

    while (pos != string::npos && i++ < firstN)
    {
        ret.push_back(tmp.substr(0, pos));
        tmp = tmp.substr(pos+1);
        pos = tmp.find(token);
    }

    ret.push_back(tmp);

    return ret;
}

}
