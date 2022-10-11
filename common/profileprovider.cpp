#include <boost/algorithm/string.hpp>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include <dirent.h> 
#include <stdio.h> 

#include "profileprovider.h"
#include "logger.h"
#include "table.h"
#include "schema.h"

using namespace std;
using namespace swss;

ProfileProvider& ProfileProvider::instance()
{
    static ProfileProvider instance;
    return instance;
}

shared_ptr<string> ProfileProvider::getConfig(const string &table, const string &key, const string &field, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());
    assert(!field.empty());

    auto staticConfig = getConfigs(table, key, cfgDbConnector);
    auto result = staticConfig.find(field);
    if (result != staticConfig.end())
    {
        return make_shared<string>(result->second);
    }

    // Config not found is different with 'empty' config
    return nullptr;
}

bool ProfileProvider::appendConfigs(const string &table, const string &key, vector<pair<string, string> > &values, DBConnector* cfgDbConnector)
{
    assert(!table.empty());
    assert(!key.empty());

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), key.c_str());

    auto staticConfig = getConfigs(table, key, cfgDbConnector);

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

map<string, string> ProfileProvider::getConfigs(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    if (itemDeleted(table, key, cfgDbConnector))
    {
        SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs item %s %s deleted.\n", table.c_str(), key.c_str());
        map<string, string> map;
        return map;
    }

    auto& staticCfgDbConnector = getStaticCfgDBConnector(cfgDbConnector);
    auto itemkey = getKeyName(table, key, &staticCfgDbConnector);
    return staticCfgDbConnector.hgetall<map<string, string>>(itemkey);
}

map<string, map<string, map<string, string>>> ProfileProvider::getConfigs(DBConnector* cfgDbConnector)
{
    auto& staticCfgDbConnector = getStaticCfgDBConnector(cfgDbConnector);
    auto configs = staticCfgDbConnector.getall();
    
    // If a profile item mark as 'deleted', it's shoud not exist in result.
    list<pair<string, string>> deletedItems;
    for(auto const& tableItem: configs)
    {
        auto table = tableItem.first;
        for(auto const& item: tableItem.second)
        {
            auto key = item.first;
            if (itemDeleted(table, key, cfgDbConnector))
            {
                SWSS_LOG_DEBUG("DefaultValueProvider::GetConfigs item %s %s deleted.\n", table.c_str(), key.c_str());
                deletedItems.push_back(make_pair(table, key));
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

vector<string> ProfileProvider::getKeys(const string &table, DBConnector* cfgDbConnector)
{
    auto& staticCfgDbConnector = getStaticCfgDBConnector(cfgDbConnector);
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
        if (!itemDeleted(table, row, cfgDbConnector))
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

ProfileProvider::ProfileProvider()
{
}

ProfileProvider::~ProfileProvider()
{
}

bool ProfileProvider::tryRevertItem(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    if (itemDeleted(table, key, cfgDbConnector))
    {
        revertItem(table, key, cfgDbConnector);
        return true;
    }

    return false;
}

bool ProfileProvider::tryDeleteItem(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    if (!itemDeleted(table, key, cfgDbConnector))
    {
        deleteItem(table, key, cfgDbConnector);
        return true;
    }

    return false;
}

bool ProfileProvider::itemDeleted(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    auto deletedkey = getDeletedKeyName(table, key, cfgDbConnector);
    return cfgDbConnector->exists(deletedkey) == true;
}

void ProfileProvider::deleteItem(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    auto deletedkey = getDeletedKeyName(table, key, cfgDbConnector);
    // Only need deletedkey to mark the item is deleted.
    cfgDbConnector->hset(deletedkey, "", "");
}

void ProfileProvider::revertItem(const string &table, const string &key, DBConnector* cfgDbConnector)
{
    auto deletedkey = getDeletedKeyName(table, key,cfgDbConnector);
    cfgDbConnector->del(deletedkey);
}

DBConnector& ProfileProvider::getStaticCfgDBConnector(DBConnector* cfgDbConnector)
{
    auto ns = cfgDbConnector->getNamespace();
    auto result = m_staticCfgDBMap.find(ns);
    if (result != m_staticCfgDBMap.end())
    {
        return result->second;
    }

    // Create new DBConnector instance to PROFILE_DB
    const string staticDbName = "PROFILE_DB";
    m_staticCfgDBMap.emplace(piecewise_construct,
              forward_as_tuple(ns),
              forward_as_tuple(staticDbName, SonicDBConfig::getDbId(staticDbName, ns), true, ns));

    result = m_staticCfgDBMap.find(ns);
    return result->second;
}