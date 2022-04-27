#include <boost/algorithm/string.hpp>
#include <cassert>
#include <iostream>

#include "defaultvalueprovider.h"
#include "dbdecorator.h"
#include "logger.h"
#include "table.h"

using namespace std;
using namespace swss;

#define POS(TUPLE)      get<0>(TUPLE)
#define TABLE(TUPLE)    get<1>(TUPLE)
#define ROW(TUPLE)      get<2>(TUPLE)

ConfigDBDecorator::ConfigDBDecorator(string separator)
    :m_separator(separator)
{
}


template<typename OutputIterator>
void ConfigDBDecorator::_decorate(const std::string &key, OutputIterator &result)
{
    auto tableAndRow = getTableAndRow(key);
    if (POS(tableAndRow) == string::npos)
    {
        return;
    }
    
    DefaultValueProvider::Instance().AppendDefaultValues(TABLE(tableAndRow), ROW(tableAndRow), result);
}

void ConfigDBDecorator::decorate(const string &key, vector<FieldValueTuple> &result)
{
    _decorate(key, result);
}

void ConfigDBDecorator::decorate(const string &key, map<string, string> &result)
{
    _decorate(key, result);
}

template<typename OutputIterator>
void ConfigDBDecorator::_decorate(const std::string &key, redisReply *&ctx, OutputIterator &result)
{
    auto tableAndRow = getTableAndRow(key);
    if (POS(tableAndRow) == string::npos)
    {
        return;
    }

    // When DB ID is CONFIG_DB, append default value to config DB result.
    map<string, string> valuesWithDefault;
    map<string, string> existedValues;
    for (unsigned int i = 0; i < ctx->elements; i += 2)
    {
        existedValues[ctx->element[i]->str] = ctx->element[i+1]->str;
        valuesWithDefault[ctx->element[i]->str] = ctx->element[i+1]->str;
    }

    DefaultValueProvider::Instance().AppendDefaultValues(TABLE(tableAndRow), ROW(tableAndRow), valuesWithDefault);

    for (auto& fieldValuePair : valuesWithDefault)
    {
        auto findResult = existedValues.find(fieldValuePair.first);
        if (findResult == existedValues.end())
        {
            *result = make_pair(fieldValuePair.first, fieldValuePair.second);
            ++result;
        }
    }
}

void ConfigDBDecorator::decorate(const string &key, redisReply *&ctx, insert_iterator<unordered_map<string, string> > &result)
{
    _decorate(key, ctx, result);
}

void ConfigDBDecorator::decorate(const string &key, redisReply *&ctx, insert_iterator<map<string, string> > &result)
{
    _decorate(key, ctx, result);
}

shared_ptr<string> ConfigDBDecorator::decorate(const string &key, const string &field)
{
    auto tableAndRow = getTableAndRow(key);
    if (POS(tableAndRow) == string::npos)
    {
        return shared_ptr<string>(NULL);
    }

    return DefaultValueProvider::Instance().GetDefaultValue(TABLE(tableAndRow), ROW(tableAndRow), field);
}

tuple<size_t, string, string> ConfigDBDecorator::getTableAndRow(const string &key)
{
    string table;
    string row;
    size_t pos = key.find(m_separator);
    if (pos == string::npos)
    {
        SWSS_LOG_WARN("Table::get key for config DB is %s, can't find a sepreator\n", key.c_str());
    }
    else
    {
        table = key.substr(0, pos);
        row = key.substr(pos + 1);
    }

    return tuple<size_t, string,string>(pos, table, row);
}

shared_ptr<DBDecorator> ConfigDBDecorator::Create(string separator)
{
    return shared_ptr<DBDecorator>(new ConfigDBDecorator(separator));
}