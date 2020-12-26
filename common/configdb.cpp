#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include "configdb.h"
#include "pubsub.h"
#include "converter.h"

using namespace std;
using namespace swss;

ConfigDBConnector::ConfigDBConnector(bool use_unix_socket_path, const char *netns)
    : SonicV2Connector(use_unix_socket_path, netns)
{
}

void ConfigDBConnector::connect(bool wait_for_init, bool retry_on)
{
    m_db_name = "CONFIG_DB";
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
                    string key = channel.substr(pos + 1);
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
