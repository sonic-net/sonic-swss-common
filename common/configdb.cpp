#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <vector>
#include "configdb.h"
#include "pubsub.h"
#include "converter.h"

using namespace std;
using namespace swss;

ConfigDBConnector::ConfigDBConnector(bool use_unix_socket_path, const char *netns)
    : SonicV2Connector(use_unix_socket_path, netns)
    , TABLE_NAME_SEPARATOR("|")
    , KEY_SEPARATOR("|")
{
}

void ConfigDBConnector::db_connect(string db_name, bool wait_for_init, bool retry_on)
{
    m_db_name = db_name;
    KEY_SEPARATOR = TABLE_NAME_SEPARATOR = get_db_separator(db_name);
    SonicV2Connector::connect(m_db_name, retry_on);

    if (wait_for_init)
    {
        auto& client = get_redis_client(m_db_name);
        auto pubsub = client.pubsub();
        auto initialized = client.get(INIT_INDICATOR);
        if (initialized)
        {
            string pattern = "__keyspace@" + to_string(get_dbid(m_db_name)) +  "__:" + INIT_INDICATOR;
            pubsub->psubscribe(pattern);
            for (;;)
            {
                auto item = pubsub->listen_message();
                if (item["type"] == "pmessage")
                {
                    string channel = item["channel"];
                    size_t pos = channel.find(':');
                    string key;
                    if (pos != string::npos)
                    {
                        key = channel.substr(pos + 1);
                    }
                    if (key == INIT_INDICATOR)
                    {
                        initialized = client.get(INIT_INDICATOR);
                        if (initialized && !initialized->empty())
                        {
                            break;
                        }
                    }
                }
            }
            pubsub->punsubscribe(pattern);
        }
    }
}

void ConfigDBConnector::connect(bool wait_for_init, bool retry_on)
{
    db_connect("CONFIG_DB", wait_for_init, retry_on);
}

// Write a table entry to config db.
//    Remove extra fields in the db which are not in the data.
// Args:
//     table: Table name.
//     key: Key of table entry, or a tuple of keys if it is a multi-key table.
//     data: Table row data in a form of dictionary {'column_key': 'value', ...}.
//           Pass {} as data will delete the entry.
void ConfigDBConnector::set_entry(string table, string key, const unordered_map<string, string>& data)
{
    auto& client = get_redis_client(m_db_name);
    string _hash = to_upper(table) + TABLE_NAME_SEPARATOR + key;
    if (data.empty())
    {
        client.del(_hash);
    }
    else
    {
        auto original = get_entry(table, key);
        client.hmset(_hash, data.begin(), data.end());
        for (auto& it: original)
        {
            auto& k = it.first;
            bool found = data.find(k) == data.end();
            if (!found)
            {
                client.hdel(_hash, k);
            }
        }
    }
}

// Modify a table entry to config db.
// Args:
//     table: Table name.
//     key: Key of table entry, or a tuple of keys if it is a multi-key table.
//     data: Table row data in a form of dictionary {'column_key': 'value', ...}.
//           Pass {} as data will create an entry with no column if not already existed.
//           Pass None as data will delete the entry.
void ConfigDBConnector::mod_entry(string table, string key, const unordered_map<string, string>& data)
{
    auto& client = get_redis_client(m_db_name);
    string _hash = to_upper(table) + TABLE_NAME_SEPARATOR + key;
    if (data.empty())
    {
        client.del(_hash);
    }
    else
    {
        client.hmset(_hash, data.begin(), data.end());
    }
}

// Read a table entry from config db.
// Args:
//     table: Table name.
//     key: Key of table entry, or a tuple of keys if it is a multi-key table.
// Returns:
//     Table row data in a form of dictionary {'column_key': 'value', ...}
//     Empty dictionary if table does not exist or entry does not exist.
unordered_map<string, string> ConfigDBConnector::get_entry(string table, string key)
{
    auto& client = get_redis_client(m_db_name);
    string _hash = to_upper(table) + TABLE_NAME_SEPARATOR + key;
    return client.hgetall(_hash);
}

// Read all keys of a table from config db.
// Args:
//     table: Table name.
//     split: split the first part and return second.
//            Useful for keys with two parts <tablename>:<key>
// Returns:
//     List of keys.
vector<string> ConfigDBConnector::get_keys(string table, bool split)
{
    auto& client = get_redis_client(m_db_name);
    string pattern = to_upper(table) + TABLE_NAME_SEPARATOR + "*";
    const auto& keys = client.keys(pattern);
    vector<string> data;
    for (auto& key: keys)
    {
        if (split)
        {
            size_t pos = key.find(TABLE_NAME_SEPARATOR);
            string row;
            if (pos != string::npos)
            {
                row = key.substr(pos + 1);
            }
            data.push_back(row);
        }
        else
        {
            data.push_back(key);
        }
    }
    return data;
}

// Read an entire table from config db.
// Args:
//     table: Table name.
// Returns:
//     Table data in a dictionary form of
//     { 'row_key': {'column_key': value, ...}, ...}
//     or { ('l1_key', 'l2_key', ...): {'column_key': value, ...}, ...} for a multi-key table.
//     Empty dictionary if table does not exist.
unordered_map<string, unordered_map<string, string>> ConfigDBConnector::get_table(string table)
{
    auto& client = get_redis_client(m_db_name);
    string pattern = to_upper(table) + TABLE_NAME_SEPARATOR + "*";
    const auto& keys = client.keys(pattern);
    unordered_map<string, unordered_map<string, string>> data;
    for (auto& key: keys)
    {
        auto const& entry = client.hgetall(key);
        size_t pos = key.find(TABLE_NAME_SEPARATOR);
        string row;
        if (pos != string::npos)
        {
            row = key.substr(pos + 1);
        }
        data[row] = entry;
    }
    return data;
}

// Delete an entire table from config db.
// Args:
//     table: Table name.
void ConfigDBConnector::delete_table(string table)
{
    auto& client = get_redis_client(m_db_name);
    string pattern = to_upper(table) + TABLE_NAME_SEPARATOR + "*";
    const auto& keys = client.keys(pattern);
    for (auto& key: keys)
    {
        client.del(key);
    }
}

// Write multiple tables into config db.
//    Extra entries/fields in the db which are not in the data are kept.
// Args:
//     data: config data in a dictionary form
//     {
//         'TABLE_NAME': { 'row_key': {'column_key': 'value', ...}, ...},
//         'MULTI_KEY_TABLE_NAME': { ('l1_key', 'l2_key', ...) : {'column_key': 'value', ...}, ...},
//         ...
//     }
void ConfigDBConnector::mod_config(const unordered_map<string, unordered_map<string, unordered_map<string, string>>>& data)
{
    for (auto const& it: data)
    {
        string table_name = it.first;
        auto const& table_data = it.second;
        if (table_data.empty())
        {
            delete_table(table_name);
            continue;
        }
        for (auto const& ie: table_data)
        {
            string key = ie.first;
            auto const& fvs = ie.second;
            mod_entry(table_name, key, fvs);
        }
    }
}

// Read all config data.
// Returns:
//     Config data in a dictionary form of
//     {
//         'TABLE_NAME': { 'row_key': {'column_key': 'value', ...}, ...},
//         'MULTI_KEY_TABLE_NAME': { ('l1_key', 'l2_key', ...) : {'column_key': 'value', ...}, ...},
//         ...
//     }
unordered_map<string, unordered_map<string, unordered_map<string, string>>> ConfigDBConnector::get_config()
{
    auto& client = get_redis_client(m_db_name);
    auto const& keys = client.keys("*");
    unordered_map<string, unordered_map<string, unordered_map<string, string>>> data;
    for (string key: keys)
    {
        size_t pos = key.find(TABLE_NAME_SEPARATOR);
        if (pos == string::npos)
        {
            continue;
        }
        string table_name = key.substr(0, pos);
        string row = key.substr(pos + 1);
        auto const& entry = client.hgetall(key);

        if (!entry.empty())
        {
            data[table_name][row] = entry;
        }
    }
    return data;
}

std::string ConfigDBConnector::getKeySeparator() const
{
    return KEY_SEPARATOR;
}
