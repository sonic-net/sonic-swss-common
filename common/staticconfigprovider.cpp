
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

std::shared_ptr<std::string> StaticConfigProvider::GetConfig(const std::string &table, const std::string &key, const std::string &field, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());
    assert(!field.empty());
    
    // TODO: improve following POC code performance, get field from redis directly
    auto staticConfig = GetConfigs(table, key, cfgDbConnector);
    auto result = staticConfig.find(field);
    if (result != staticConfig.end())
    {
        return std::make_shared<string>(result->second);
    }
    
    // Config not found is different with 'empty' config
    return nullptr;
}

bool StaticConfigProvider::AppendConfigs(const std::string &table, const std::string &key, std::vector<std::pair<std::string, std::string> > &values, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), key.c_str());

    auto staticConfig = GetConfigs(table, key, cfgDbConnector);

    map<string, string> existedValues;
    for (auto& fieldValuePair : values)
    {
        existedValues.emplace(fieldValuePair.first, fieldValuePair.second);
    }

    bool appendValues = false;
    for (auto& fieldValuePair : staticConfig)
    {
        auto findresult = existedValues.find(fieldValuePair.first);
        if (findresult == existedValues.end())
        {
            appendValues = true;
            values.emplace_back(fieldValuePair.first, fieldValuePair.second);
        }
    }

    return appendValues;
}

bool StaticConfigProvider::AppendConfigs(const string &table, const string &key, std::map<std::string, std::string>& values, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), key.c_str());

    auto staticConfig = GetConfigs(table, key, cfgDbConnector);

    bool appendValues = false;
    for (auto& fieldValuePair : staticConfig)
    {
        auto findresult = values.find(fieldValuePair.first);
        if (findresult == values.end())
        {
            appendValues = true;
            values.emplace(fieldValuePair.first, fieldValuePair.second);
        }
    }

    return appendValues;
}

void StaticConfigProvider::AppendConfigs(const string &table, KeyOpFieldsValuesTuple &kofv_tuple, DBConnector* cfgDbConnector)
{
    if (kfvOp(kofv_tuple) != SET_COMMAND)
    {
        return;
    }

    auto& key = kfvKey(kofv_tuple);
    auto& data = kfvFieldsValues(kofv_tuple);

    auto staticConfig = GetConfigs(table, key, cfgDbConnector);

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

map<string, string> StaticConfigProvider::GetConfigs(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    if (ItemDeleted(table, key, cfgDbConnector))
    {
        SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs item %s %s deleted.\n", table.c_str(), key.c_str());
        map<string, string> map;
        return map;
    }

    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    auto itemkey = getKeyName(table, key, &staticCfgDbConnector);
    return staticCfgDbConnector.hgetall<map<string, string>>(itemkey);
}

map<string, map<string, map<string, string>>> StaticConfigProvider::GetConfigs(DBConnector* cfgDbConnector)
{
    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    auto configs = staticCfgDbConnector.getall();
    
    // If a profile item mark as 'deleted', it's shoud not exist in result.
    list<pair<string, string>> deletedItems;
    for(auto const& tableItem: configs)
    {
        auto table = tableItem.first;
        for(auto const& item: tableItem.second)
        {
            auto key = item.first;
            if (ItemDeleted(table, key, cfgDbConnector))
            {
                SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs item %s %s deleted.\n", table.c_str(), key.c_str());
                deletedItems.push_back(std::make_pair(table, key));
            }
        }
    }

    for(auto const& deletedItem: deletedItems)
    {
        auto table = deletedItem.first;
        auto key = deletedItem.second;
        SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs remove deleted item %s %s from result.\n", table.c_str(), key.c_str());
        configs[table].erase(key);
    }

    return configs;
}

vector<string> StaticConfigProvider::GetKeys(const std::string &table, DBConnector* cfgDbConnector)
{
    auto& staticCfgDbConnector = GetStaticCfgDBConnector(cfgDbConnector);
    auto pattern = getKeyName(table, "*", &staticCfgDbConnector);
    auto keys = staticCfgDbConnector.keys(pattern);
    
    const auto separator = SonicDBConfig::getSeparator(&staticCfgDbConnector);
    vector<string> result;
    for(auto const& itemKey: keys)
    {
        size_t pos = itemKey.find(separator);
        if (pos == string::npos)
        {
            SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs can't find separator %s in %s.\n", separator.c_str(), itemKey.c_str());
            continue;
        }

        auto row = itemKey.substr(pos + 1);
        if (!ItemDeleted(table, row, cfgDbConnector))
        {
            result.push_back(row);
        }
        else
        {
            SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs item %s %s deleted.\n", table.c_str(), row.c_str());
        }
    }

    return result;
}

StaticConfigProvider::StaticConfigProvider()
{
}


StaticConfigProvider::~StaticConfigProvider()
{
}

bool StaticConfigProvider::TryRevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    if (ItemDeleted(table, key, cfgDbConnector))
    {
        RevertItem(table, key, cfgDbConnector);
        return true;
    }
    
    return false;
}

bool StaticConfigProvider::TryDeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    if (!ItemDeleted(table, key, cfgDbConnector))
    {
        DeleteItem(table, key, cfgDbConnector);
        return true;
    }
    
    return false;
}

bool StaticConfigProvider::ItemDeleted(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    auto deletedkey = getDeletedKeyName(table, key, cfgDbConnector);
    return cfgDbConnector->exists(deletedkey) == true;
}

void StaticConfigProvider::DeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    auto deletedkey = getDeletedKeyName(table, key, cfgDbConnector);
    // Set value as a empty dict, because PROFILE_DELETE_TABLE is a CONFIG_DB table.
    cfgDbConnector->hset(deletedkey, "", "");
}

void StaticConfigProvider::RevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector)
{
    auto deletedkey = getDeletedKeyName(table, key,cfgDbConnector);
    cfgDbConnector->del(deletedkey);
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