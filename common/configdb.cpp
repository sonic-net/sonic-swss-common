#include <boost/algorithm/string.hpp>
#include <map>
#include <vector>
#include "configdb.h"
#include "pubsub.h"
#include "converter.h"

#include <dirent.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <libyang/libyang.h>

using namespace std;
using namespace swss;

map<string, lys_node *> table_name_to_data_mapping;

struct ly_ctx * CreateCtxAndLoadAllModules(char* module_path)
{
    struct ly_ctx *ctx = ly_ctx_new(module_path, LY_CTX_ALLIMPLEMENTED);
    DIR *module_dir = opendir(module_path);
    if (module_dir)
    {
        struct dirent *sub_dir;
        while ((sub_dir = readdir(module_dir)) != NULL)
        {
            if (sub_dir->d_type == DT_REG)
            {
                string file_name(sub_dir->d_name);
                printf("file_name: %s\n", file_name.c_str());
                auto pos = file_name.find(".yang");
                string module_name = file_name.substr(0, pos);
                printf("%s\n", module_name.c_str());

                // load module
                const struct lys_module *module = ly_ctx_load_module(ctx, module_name.c_str(), "");
                if (module->data)
                {
                    // every module should always has a top level container same with module name
                    // https://github.com/Azure/SONiC/blob/eeebbf6f8a32e5d989595b0816849e8ef0d15141/doc/mgmt/SONiC_YANG_Model_Guidelines.md
                    struct lys_node *data = module->data;
                    printf("root node name: %s\n",data->name);
                    data = data->child;
                    string container_name(data->name);
                    printf("root container name: %s\n",data->name);
                    table_name_to_data_mapping[container_name] = data;
                }
            }
        }
        closedir(module_dir);
    }
    
    return ctx;
}

void LoadAndAppendDefaultValue(string module_name, map<string, map<string, string>>& data)
{
    CreateCtxAndLoadAllModules("/home/liuh/yang/yang-models/");

    auto module_root_container = table_name_to_data_mapping.find(module_name);
    if (module_root_container == table_name_to_data_mapping.end())
    {
        return;
    }
    
    // found yang model for the table
    auto next_child = module_root_container->second->child;
    while (next_child)
    {
        printf("Child name: %s\n",next_child->name);
        switch (next_child->nodetype)
        {
            case LYS_LIST:
            {
                // Mapping tables in Redis are defined using nested 'list'. Use 'sonic-ext:map-list "true";' to indicate that the 'list' is used for mapping table. The outer 'list' is used for multiple instances of mapping. The inner 'list' is used for mapping entries for each outer list instance.
                // check if the list if for mapping ConfigDbTable
                printf("Ext count: %d\n",next_child->ext_size);
                
                auto table_column = next_child->child;
                while (table_column)
                {
                    struct lys_node_leaf *leafnode = (struct lys_node_leaf*)table_column;
                    string column_name(table_column->name);
                    printf("    table column name: %s, default: %s\n",table_column->name, leafnode->dflt);
                    
                    // assign default value to every row
                    if (leafnode->dflt)
                    {
                        string default_value(leafnode->dflt);
                        for (auto row_iterator = data.begin(); row_iterator != data.end(); ++row_iterator)
                        {
                            printf("assign default data to: %s\n", row_iterator->first.c_str());
                            auto column = row_iterator->second.find(column_name);
                            if (column == row_iterator->second.end())
                            {
                                printf("assigned default data to %s: %s\n", column_name.c_str(), default_value.c_str());
                                // when no data from config DB, assign default value to result
                                row_iterator->second[column_name] = default_value;
                            }
                        }
                    }
                    
                    table_column = table_column->next;
                }
            }
            break;
            case LYS_UNKNOWN:
            case LYS_CONTAINER:
            case LYS_CHOICE:
            case LYS_LEAF:
            case LYS_LEAFLIST:
            case LYS_ANYXML:
            case LYS_CASE:
            case LYS_NOTIF:
            case LYS_RPC:
            case LYS_INPUT:
            case LYS_OUTPUT:
            case LYS_GROUPING:
            case LYS_USES:
            case LYS_AUGMENT:
            case LYS_ACTION:
            case LYS_ANYDATA:
            case LYS_EXT:
            default:
            break;
        }

        next_child = next_child->next;
    }
}

ConfigDBConnector_Native::ConfigDBConnector_Native(bool use_unix_socket_path, const char *netns)
    : SonicV2Connector_Native(use_unix_socket_path, netns)
    , m_table_name_separator("|")
    , m_key_separator("|")
{
}

void ConfigDBConnector_Native::db_connect(string db_name, bool wait_for_init, bool retry_on)
{
    m_db_name = db_name;
    m_key_separator = m_table_name_separator = get_db_separator(db_name);
    SonicV2Connector_Native::connect(m_db_name, retry_on);

    if (wait_for_init)
    {
        auto& client = get_redis_client(m_db_name);
        auto pubsub = client.pubsub();
        auto initialized = client.get(INIT_INDICATOR);
        if (!initialized || initialized->empty())
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

void ConfigDBConnector_Native::connect(bool wait_for_init, bool retry_on)
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
void ConfigDBConnector_Native::set_entry(string table, string key, const map<string, string>& data)
{
    auto& client = get_redis_client(m_db_name);
    string _hash = to_upper(table) + m_table_name_separator + key;
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
            bool found = data.find(k) != data.end();
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
void ConfigDBConnector_Native::mod_entry(string table, string key, const map<string, string>& data)
{
    auto& client = get_redis_client(m_db_name);
    string _hash = to_upper(table) + m_table_name_separator + key;
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
map<string, string> ConfigDBConnector_Native::get_entry(string table, string key)
{
    auto& client = get_redis_client(m_db_name);
    string _hash = to_upper(table) + m_table_name_separator + key;
    return client.hgetall<map<string, string>>(_hash);
}

// Read all keys of a table from config db.
// Args:
//     table: Table name.
//     split: split the first part and return second.
//            Useful for keys with two parts <tablename>:<key>
// Returns:
//     List of keys.
vector<string> ConfigDBConnector_Native::get_keys(string table, bool split)
{
    auto& client = get_redis_client(m_db_name);
    string pattern = to_upper(table) + m_table_name_separator + "*";
    const auto& keys = client.keys(pattern);
    vector<string> data;
    for (auto& key: keys)
    {
        if (split)
        {
            size_t pos = key.find(m_table_name_separator);
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
map<string, map<string, string>> ConfigDBConnector_Native::get_table(string table)
{
    auto& client = get_redis_client(m_db_name);
    string pattern = to_upper(table) + m_table_name_separator + "*";
    const auto& keys = client.keys(pattern);
    map<string, map<string, string>> data;
    for (auto& key: keys)
    {
        auto const& entry = client.hgetall<map<string, string>>(key);
        size_t pos = key.find(m_table_name_separator);
        string row;
        if (pos == string::npos)
        {
            continue;
        }
        row = key.substr(pos + 1);
        data[row] = entry;
    }
    return data;
}

// Delete an entire table from config db.
// Args:
//     table: Table name.
void ConfigDBConnector_Native::delete_table(string table)
{
    auto& client = get_redis_client(m_db_name);
    string pattern = to_upper(table) + m_table_name_separator + "*";
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
void ConfigDBConnector_Native::mod_config(const map<string, map<string, map<string, string>>>& data)
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
map<string, map<string, map<string, string>>> ConfigDBConnector_Native::get_config()
{
    auto& client = get_redis_client(m_db_name);
    auto const& keys = client.keys("*");
    map<string, map<string, map<string, string>>> data;
    for (string key: keys)
    {
        size_t pos = key.find(m_table_name_separator);
        if (pos == string::npos) {
            continue;
        }
        string table_name = key.substr(0, pos);
        string row = key.substr(pos + 1);
        auto const& entry = client.hgetall<map<string, string>>(key);

        if (!entry.empty())
        {
            data[table_name][row] = entry;
        }
    }
    return data;
}

std::string ConfigDBConnector_Native::getKeySeparator() const
{
    return m_key_separator;
}

std::string ConfigDBConnector_Native::getTableNameSeparator() const
{
    return m_table_name_separator;
}

std::string ConfigDBConnector_Native::getDbName() const
{
    return m_db_name;
}

ConfigDBPipeConnector_Native::ConfigDBPipeConnector_Native(bool use_unix_socket_path, const char *netns)
    : ConfigDBConnector_Native(use_unix_socket_path, netns)
{
}

// Helper method to delete table entries from config db using Redis pipeline
// with batch size of REDIS_SCAN_BATCH_SIZE.
// The caller should call pipeline execute once ready
// Args:
//     client: Redis client
//     pipe: Redis DB pipe
//     pattern: key pattern
//     cursor: position to start scanning from
//
// Returns:
//     cur: poition of next item to scan
int ConfigDBPipeConnector_Native::_delete_entries(DBConnector& client, RedisTransactioner& pipe, const char *pattern, int cursor)
{
    const auto& rc = client.scan(cursor, pattern, REDIS_SCAN_BATCH_SIZE);
    auto cur = rc.first;
    auto& keys = rc.second;
    for (auto const& key: keys)
    {
        RedisCommand sdel;
        sdel.format("DEL %s", key.c_str());
        pipe.enqueue(sdel, REDIS_REPLY_INTEGER);
    }

    return cur;
}

// Helper method to delete table entries from config db using Redis pipeline.
// The caller should call pipeline execute once ready
// Args:
//     client: Redis client
//     pipe: Redis DB pipe
//     table: Table name.
void ConfigDBPipeConnector_Native::_delete_table(DBConnector& client, RedisTransactioner& pipe, string table)
{
    string pattern = to_upper(table) + m_table_name_separator + "*";
    auto cur = _delete_entries(client, pipe, pattern.c_str(), 0);
    while (cur != 0)
    {
        cur = _delete_entries(client, pipe, pattern.c_str(), cur);
    }
}

// Modify a table entry to config db.
// Args:
//     table: Table name.
//     pipe: Redis DB pipe
//     table: Table name.
//     key: Key of table entry, or a tuple of keys if it is a multi-key table.
//     data: Table row data in a form of dictionary {'column_key': 'value', ...}.
//           Pass {} as data will create an entry with no column if not already existed.
//           Pass None as data will delete the entry.
void ConfigDBPipeConnector_Native::_mod_entry(RedisTransactioner& pipe, string table, string key, const map<string, string>& data)
{
    string _hash = to_upper(table) + m_table_name_separator + key;
    if (data.empty())
    {
        RedisCommand sdel;
        sdel.format("DEL %s", _hash.c_str());
        pipe.enqueue(sdel, REDIS_REPLY_INTEGER);
    }
    else
    {
        RedisCommand shmset;
        shmset.formatHMSET(_hash, data.begin(), data.end());
        pipe.enqueue(shmset, REDIS_REPLY_STATUS);
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
void ConfigDBPipeConnector_Native::mod_config(const map<string, map<string, map<string, string>>>& data)
{
    auto& client = get_redis_client(m_db_name);
    DBConnector clientPipe(client);
    RedisTransactioner pipe(&clientPipe);
    pipe.multi();
    for (auto const& id: data)
    {
        auto& table_name = id.first;
        auto& table_data = id.second;
        if (table_data.empty())
        {
            _delete_table(client, pipe, table_name);
            continue;
        }
        for (auto const& it: table_data)
        {
            auto& key = it.first;
            _mod_entry(pipe, table_name, key, it.second);
        }
    }
    pipe.exec();
}

// Read config data in batches of size REDIS_SCAN_BATCH_SIZE using Redis pipelines
// Args:
//     client: Redis client
//     pipe: Redis DB pipe
//     data: config dictionary
//     cursor: position to start scanning from
//
// Returns:
//     cur: position of next item to scan
int ConfigDBPipeConnector_Native::_get_config(DBConnector& client, RedisTransactioner& pipe, map<string, map<string, map<string, string>>>& data, int cursor)
{
    auto const& rc = client.scan(cursor, "*", REDIS_SCAN_BATCH_SIZE);
    auto cur = rc.first;
    auto const& keys = rc.second;
    pipe.multi();
    for (auto const& key: keys)
    {
        size_t pos = key.find(m_table_name_separator);
        if (pos == string::npos)
        {
            continue;
        }
        RedisCommand shgetall;
        shgetall.format("HGETALL %s", key.c_str());
        pipe.enqueue(shgetall, REDIS_REPLY_ARRAY);
    }
    pipe.exec();

    for (auto const& key: keys)
    {
        size_t pos = key.find(m_table_name_separator);
        string table_name = key.substr(0, pos);
        string row;
        if (pos == string::npos)
        {
            continue;
        }
        row = key.substr(pos + 1);

        auto reply = pipe.dequeueReply();
        RedisReply r(reply);

        auto& dataentry = data[table_name][row];
        for (unsigned int i = 0; i < r.getChildCount(); i += 2)
        {
            string field = r.getChild(i)->str;
            string value = r.getChild(i+1)->str;
            dataentry.emplace(field, value);
        }
    }
    return cur;
}

map<string, map<string, map<string, string>>> ConfigDBPipeConnector_Native::get_config()
{
    auto& client = get_redis_client(m_db_name);
    DBConnector clientPipe(client);
    RedisTransactioner pipe(&clientPipe);

    map<string, map<string, map<string, string>>> data;
    auto cur = _get_config(client, pipe, data, 0);
    while (cur != 0)
    {
        cur = _get_config(client, pipe, data, cur);
    }

    return data;
}
