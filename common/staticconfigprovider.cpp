
#include <boost/algorithm/string.hpp>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include <dirent.h> 
#include <stdio.h> 

#include <libyang/libyang.h>

#include "staticconfigprovider.h"
#include "logger.h"
#include "table.h"
#include "schema.h"

using namespace std;
using namespace swss;

StaticConfigProvider& StaticConfigProvider::Instance()
{
    static StaticConfigProvider instance;
    return instance;
}

std::shared_ptr<std::string> StaticConfigProvider::GetConfig(const std::string &table, const std::string &key, std::string field, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());
    assert(!field.empty());
    
    // TODO: improve following POC code performance, get field from redis directly
    auto staticConfig = GetStaticConfig(table, key, cfgDbConnector);
    auto result = staticConfig.find(field);
    if (result != staticConfig.end())
    {
        return std::make_shared<string>(result->second);
    }
    
    return nullptr;
}

void StaticConfigProvider::AppendConfigs(const std::string &table, const std::string &key, std::vector<std::pair<std::string, std::string> > &values, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), key.c_str());
#ifdef DEBUG
    if (!FeatureEnabledByEnvironmentVariable())
    {
        return;
    }
#endif

    auto staticConfig = GetStaticConfig(table, key, cfgDbConnector);

    map<string, string> existedValues;
    for (auto& fieldValuePair : values)
    {
        existedValues.emplace(fieldValuePair.first, fieldValuePair.second);
    }

    for (auto& fieldValuePair : staticConfig)
    {
        auto findresult = existedValues.find(fieldValuePair.first);
        if (findresult == existedValues.end())
        {
            values.emplace_back(fieldValuePair.first, fieldValuePair.second);
        }
    }
}

void StaticConfigProvider::AppendConfigs(const string &table, const string &key, std::map<std::string, std::string>& values, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), key.c_str());
#ifdef DEBUG
    if (!FeatureEnabledByEnvironmentVariable())
    {
        return;
    }
#endif

    auto staticConfig = GetStaticConfig(table, key, cfgDbConnector);

    for (auto& fieldValuePair : staticConfig)
    {
        auto findresult = values.find(fieldValuePair.first);
        if (findresult == values.end())
        {
            values.emplace(fieldValuePair.first, fieldValuePair.second);
        }
    }
}

void StaticConfigProvider::AppendConfigs(const string &table, KeyOpFieldsValuesTuple &kofv_tuple, DBConnector* cfgDbConnector)
{
    if (kfvOp(kofv_tuple) != SET_COMMAND)
    {
        return;
    }

    auto& key = kfvKey(kofv_tuple);
    auto& data = kfvFieldsValues(kofv_tuple);

    auto staticConfig = GetStaticConfig(table, key, cfgDbConnector);

    // TODO: following code dupe with default value provider, improve code by reuse it.
    for (auto& fieldValuePair : staticConfig)
    {
        auto field = fieldValuePair.first;
        bool append = true;
        for (auto& kvpair : data)
        {
            auto current_field = fvField(kvpair);
            if (current_field == field)
            {
                append = false;
                break;
            }
        }

        if (append)
        {
            data.push_back(FieldValueTuple(field, fieldValuePair.second));
        }
    }
}

std::map<std::string, std::string> StaticConfigProvider::GetConfigs(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    std::map<std::string, std::string> result;
    AppendConfigs(table, key, result, cfgDbConnector);
    return result;
}

vector<string> StaticConfigProvider::GetKeys(const std::string &table, DBConnector* cfgDbConnector)
{
    auto pattern = getKeyName(table, "*");
    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    return staticCfgDbConnector.keys(pattern);
}

StaticConfigProvider::StaticConfigProvider()
{
}


StaticConfigProvider::~StaticConfigProvider()
{
}


unordered_map<string, string> StaticConfigProvider::GetStaticConfig(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    auto itemkey = getKeyName(table, key);
    auto deletedkey = getKeyName(PROFILE_DELETE_TABLE, itemkey);
    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    if (ItemDeleted(deletedkey, staticCfgDbConnector))
    {
        unordered_map<string, string> map;
        return map;
    }
    
    return staticCfgDbConnector.hgetall<unordered_map<string, string>>(itemkey);
}


bool StaticConfigProvider::TryRevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    auto itemkey = getKeyName(table, key);
    auto deletedkey = getKeyName(PROFILE_DELETE_TABLE, itemkey);
    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    if (ItemDeleted(deletedkey, staticCfgDbConnector))
    {
        RevertItem(deletedkey, staticCfgDbConnector);
        return true;
    }
    
    return false;
}

bool StaticConfigProvider::TryDeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    auto itemkey = getKeyName(table, key);
    auto deletedkey = getKeyName(PROFILE_DELETE_TABLE, itemkey);
    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    if (!ItemDeleted(deletedkey, staticCfgDbConnector))
    {
        DeleteItem(deletedkey, staticCfgDbConnector);
        return true;
    }
    
    return false;
}

bool StaticConfigProvider::ItemDeleted(const std::string &key, DBConnector& staticCfgDbConnector)
{
    auto deletedkey = getKeyName(PROFILE_DELETE_TABLE, key);
    return staticCfgDbConnector.exists(deletedkey) == false;
}

void StaticConfigProvider::DeleteItem(const std::string &key, DBConnector& staticCfgDbConnector)
{
    auto deletedkey = getKeyName(PROFILE_DELETE_TABLE, key);
    staticCfgDbConnector.set(deletedkey, "");
}

void StaticConfigProvider::RevertItem(const std::string &key, DBConnector& staticCfgDbConnector)
{
    auto deletedkey = getKeyName(PROFILE_DELETE_TABLE, key);
    staticCfgDbConnector.del(deletedkey);
}

DBConnector& StaticConfigProvider::GetStaticCfgDBConnector(DBConnector* cfgDbConnector)
{
    auto ns = cfgDbConnector->getNamespace();
    auto result = m_staticCfgDBMap.find(ns);
    if (result != m_staticCfgDBMap.end())
    {
        return result->second;
    }

    // create new DBConnector instance
    const string staticDbName = "PROFILE_DB";
    m_staticCfgDBMap.emplace(std::piecewise_construct,
              std::forward_as_tuple(ns),
              std::forward_as_tuple(staticDbName, SonicDBConfig::getDbId(staticDbName, ns), true, ns));

    result = m_staticCfgDBMap.find(ns);
    return result->second;
}