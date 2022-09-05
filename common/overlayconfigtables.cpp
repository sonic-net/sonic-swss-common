#include <boost/algorithm/string.hpp>

#include "common/overlayconfigtables.h"
#include "common/defaultvalueprovider.h"
#include "common/staticconfigprovider.h"

using namespace std;
using namespace swss;

OverlayTable::OverlayTable(const DBConnector *db, const string &tableName)
    : Table(db, tableName)
{
}

OverlayTable::OverlayTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
:Table(pipeline, tableName, buffered)
{
}

OverlayTable::~OverlayTable()
{
}


/* Get all the field-value tuple of the table entry with the key */
bool OverlayTable::get(const string &key, vector<pair<string, string>> &ovalues)
{
    bool result = Table::get(key, ovalues);
    auto connector = const_cast<DBConnector*>(m_pipe->getDBConnector());
    auto table_name = getTableName();

    // append static configs
    bool appendProfile = StaticConfigProvider::Instance().AppendConfigs(table_name, key, ovalues, connector);
    if (!result && appendProfile)
    {
        // No user config on this key, but found profile config on this key.
        result = true;
    }

    // append default values
    DefaultValueProvider::Instance().AppendDefaultValues(table_name, key, ovalues);

    return result;
}

bool OverlayTable::hget(const string &key, const string &field,  string &value)
{
    auto result = Table::hget(key,
                        field,
                        value);
    if (result)
    {
        return true;
    }

    auto connector = const_cast<DBConnector*>(m_pipe->getDBConnector());
    auto table_name = getTableName();

    // try append static configs
    auto static_config = StaticConfigProvider::Instance().GetConfig(table_name, key, field, connector);
    if (static_config != nullptr)
    {
        value = *static_config;
        return true;
    }

    // try append default values
    auto default_value = DefaultValueProvider::Instance().GetDefaultValue(table_name, key, field);
    if (default_value != nullptr)
    {
        value = *default_value;
        return true;
    }

    return false;
}

/* get all the keys in the table */
void OverlayTable::getKeys(vector<string> &keys)
{
    Table::getKeys(keys);
    
    // append static keys
    // TODO: POC, improve performance
    auto connector = const_cast<DBConnector*>(m_pipe->getDBConnector());
    auto table_name = getTableName();
    auto static_keys = StaticConfigProvider::Instance().GetKeys(table_name, connector);
    for (auto &static_key : static_keys)
    {
        if(find(keys.begin(), keys.end(), static_key) == keys.end())
        {
            keys.emplace_back(static_key);
        }
    }
}

/* Set an entry in the DB directly and configure ttl for it (op not in use) */
void OverlayTable::set(const std::string &key,
                 const std::vector<FieldValueTuple> &values, 
                 const std::string &op,
                 const std::string &prefix,
                 const int64_t &ttl)
{
    auto connector = const_cast<DBConnector*>(m_pipe->getDBConnector());
    auto table_name = getTableName();
    if (values.size())
    {
        StaticConfigProvider::Instance().TryRevertItem(table_name, key, connector);
    }
    else
    {
        // set a entry to empty will delete entry.
        StaticConfigProvider::Instance().TryDeleteItem(table_name, key, connector);
    }

    Table::set(key,
                 values, 
                 op,
                 prefix,
                 ttl);
}

/* Delete an entry in the table */
void OverlayTable::del(const std::string &key,
                 const std::string &op,
                 const std::string &prefix)
{
    auto connector = const_cast<DBConnector*>(m_pipe->getDBConnector());
    auto table_name = getTableName();
    StaticConfigProvider::Instance().TryDeleteItem(table_name, key, connector);

    Table::del(key,
                 op,
                 prefix);
}

void OverlayTable::hset(const std::string &key,
                 const std::string &field,
                 const std::string &value,
                 const std::string &op,
                 const std::string &prefix)
{
    auto connector = const_cast<DBConnector*>(m_pipe->getDBConnector());
    auto table_name = getTableName();
    StaticConfigProvider::Instance().TryRevertItem(table_name, key, connector);

    Table::hset(key,
                 field,
                 value,
                 op,
                 prefix);
}