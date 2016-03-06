#include "common/json.h"
#include <sstream>
#include <limits>

using namespace std;

namespace swss {

static const string gHex = "0123456789ABCDEF";
static bool shouldEncode(char ch)
{
    return ch == '{' || ch == '}' || ch == '\"' || ch == '\'' || ch == '%';
}

static string escape(const string &a)
{
    string ret;
    for (char i : a)
    {
        if (shouldEncode(i))
            ret += string("%") + gHex[i >> 4] + gHex[i & 0xF];
        else
            ret += i;
    }
    return ret;
}

static string descape(const string &a)
{
    string ret;
    for (auto i = a.begin(); i != a.end(); i++)
    {
        if (*i != '%')
        {
            ret+= *i;
            continue;
        }

        i++;
        ret += char(((gHex.find(*i++)) << 4) | gHex.find(*i));
    }
    return ret;
}

string JSon::buildJson(const vector<FieldValueTuple> &fv)
{
    string tab = "  ";
    string json = "{" + tab;

    //for (FieldValueTuple &i : fv)
    for (auto i = fv.begin(); i != fv.end();)
    {
        json+= tab + "\"" + escape(fvField(*i)) + "\" : \"" +
               escape(fvValue(*i)) + "\"";
        if (++i != fv.end())
            json += "," + tab;
    }
    json += tab + "}";
    return json;
}

void JSon::readJson(const string &json, vector<FieldValueTuple> &fv)
{
    istringstream f(json);
    string field, value, tmp;
    FieldValueTuple e;

    /* Read the spaces and charactes until { "field" = "value", */
    while (getline(f, tmp, '\"')) {
        /* Read field name */
        if (!getline(f, field, '\"')) break;
        fvField(e) = descape(field);
        /* Read " = " */
        if (!getline(f, tmp, '\"')) break;
        /* Read value name */
        if (!getline(f, value, '\"')) break;
        fvValue(e) = descape(value);
        fv.push_back(e);
    }
}

}
