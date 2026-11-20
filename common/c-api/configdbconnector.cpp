#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <map>
#include <vector>

#include "../configdb.h"
#include "configdbconnector.h"
#include "util.h"

using namespace swss;
using namespace std;

struct SWSSConfigDBPipeConnectorOpaque
{
    SWSSConfigDBPipeConnectorOpaque(bool use_unix_socket_path, const char *netns)
        : connector(use_unix_socket_path, netns)
    {
    }

    ConfigDBPipeConnector_Native connector;
};

namespace {

map<string, map<string, map<string, string>>> copyConfigMap(const SWSSConfigMap *config)
{
    if (config == nullptr)
    {
        throw invalid_argument("CONFIG_DB configuration map is null");
    }
    if (config->len != 0 && config->data == nullptr)
    {
        throw invalid_argument("CONFIG_DB configuration table array is null");
    }

    map<string, map<string, map<string, string>>> result;
    for (uint64_t table_index = 0; table_index < config->len; ++table_index)
    {
        const auto& table = config->data[table_index];
        if (table.table_name == nullptr)
        {
            throw invalid_argument("CONFIG_DB table name is null");
        }
        if (table.entries.len != 0 && table.entries.data == nullptr)
        {
            throw invalid_argument("CONFIG_DB row array is null");
        }

        auto& rows = result[string(table.table_name)];
        for (uint64_t row_index = 0; row_index < table.entries.len; ++row_index)
        {
            const auto& row = table.entries.data[row_index];
            if (row.key == nullptr)
            {
                throw invalid_argument("CONFIG_DB row key is null");
            }
            if (row.fieldValues.len != 0 && row.fieldValues.data == nullptr)
            {
                throw invalid_argument("CONFIG_DB field array is null");
            }

            auto& fields = rows[string(row.key)];
            for (uint64_t field_index = 0; field_index < row.fieldValues.len; ++field_index)
            {
                const auto& field = row.fieldValues.data[field_index];
                if (field.field == nullptr || field.value == nullptr)
                {
                    throw invalid_argument("CONFIG_DB field name or value is null");
                }

                auto value = reinterpret_cast<SWSSStrRef>(field.value);
                fields[string(field.field)] = string(
                    SWSSStrRef_c_str(value),
                    numeric_cast<string::size_type>(SWSSStrRef_length(value)));
            }
        }
    }
    return result;
}

SWSSConfigMap makeConfigMap(map<string, map<string, map<string, string>>> configMap)
{
    vector<SWSSConfigTable> configTables;
    configTables.reserve(configMap.size());

    for (auto& tableEntry : configMap)
    {
        SWSSConfigTable configTable;
        configTable.table_name = strdup(tableEntry.first.c_str());

        vector<SWSSKeyOpFieldValues> tableEntries;
        tableEntries.reserve(tableEntry.second.size());
        for (auto& keyEntry : tableEntry.second)
        {
            SWSSKeyOpFieldValues kfvEntry;
            kfvEntry.key = strdup(keyEntry.first.c_str());
            kfvEntry.operation = SWSSKeyOperation_SET;

            vector<pair<string, string>> pairs;
            pairs.reserve(keyEntry.second.size());
            for (auto& fieldPair : keyEntry.second)
            {
                pairs.push_back(make_pair(fieldPair.first, move(fieldPair.second)));
            }
            kfvEntry.fieldValues = makeFieldValueArray(move(pairs));
            tableEntries.push_back(kfvEntry);
        }

        auto entries = new SWSSKeyOpFieldValues[tableEntries.size()];
        copy(tableEntries.begin(), tableEntries.end(), entries);
        configTable.entries.len = numeric_cast<uint64_t>(tableEntries.size());
        configTable.entries.data = entries;
        configTables.push_back(configTable);
    }

    auto data = new SWSSConfigTable[configTables.size()];
    copy(configTables.begin(), configTables.end(), data);
    SWSSConfigMap result;
    result.len = numeric_cast<uint64_t>(configTables.size());
    result.data = data;
    return result;
}

} // namespace

SWSSResult SWSSConfigDBConnector_new(uint8_t use_unix_socket_path, const char *netns, SWSSConfigDBConnector *outConfigDb) {
    SWSSTry({
        string netns_str = netns ? string(netns) : "";
        *outConfigDb = (SWSSConfigDBConnector) new ConfigDBConnector_Native(use_unix_socket_path != 0, netns_str.c_str());
    });
}

SWSSResult SWSSConfigDBConnector_new_with_auth(uint8_t use_unix_socket_path, const char *netns,
    uint8_t authenticate, SWSSConfigDBConnector *outConnector) {
    SWSSTry({
        *outConnector = (SWSSConfigDBConnector) new ConfigDBConnector_Native(use_unix_socket_path != 0,
            netns ? netns : "", authenticate != 0);
    });
}

SWSSResult SWSSConfigDBConnector_free(SWSSConfigDBConnector configDb) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
    SWSSTry(delete (ConfigDBConnector_Native *)configDb);
#pragma GCC diagnostic pop
}

SWSSResult SWSSConfigDBConnector_connect(SWSSConfigDBConnector configDb, uint8_t wait_for_init, uint8_t retry_on) {
    SWSSTry(((ConfigDBConnector_Native *)configDb)->connect(wait_for_init != 0, retry_on != 0));
}

SWSSResult SWSSConfigDBConnector_get_entry(SWSSConfigDBConnector configDb, const char *table, const char *key, SWSSFieldValueArray *outEntry) {
    SWSSTry({
        auto entry_map = ((ConfigDBConnector_Native *)configDb)->get_entry(string(table), string(key));
        // Convert map<string, string> to vector<pair<string, string>> for makeFieldValueArray
        vector<pair<string, string>> pairs;
        pairs.reserve(entry_map.size());
        for (auto &pair : entry_map) {
            pairs.push_back(make_pair(pair.first, move(pair.second)));
        }

        *outEntry = makeFieldValueArray(std::move(pairs));
    });
}

SWSSResult SWSSConfigDBConnector_get_keys(SWSSConfigDBConnector configDb, const char *table, uint8_t split, SWSSStringArray *outKeys) {
    SWSSTry({
        auto keys = ((ConfigDBConnector_Native *)configDb)->get_keys(string(table), split != 0);
        *outKeys = makeStringArray(std::move(keys));
    });
}

SWSSResult SWSSConfigDBConnector_get_table(SWSSConfigDBConnector configDb, const char *table, SWSSKeyOpFieldValuesArray *outTable) {
    SWSSTry({
        auto table_map = ((ConfigDBConnector_Native *)configDb)->get_table(string(table));

        // Convert map<string, map<string, string>> to vector<SWSSKeyOpFieldValues>
        vector<SWSSKeyOpFieldValues> table_entries;
        table_entries.reserve(table_map.size());

        for (auto &entry : table_map) {
            SWSSKeyOpFieldValues kfv_entry;
            kfv_entry.key = strdup(entry.first.c_str());
            kfv_entry.operation = SWSSKeyOperation_SET; // ConfigDB entries are always SET operations

            // Convert inner map to field-value array
            vector<pair<string, string>> pairs;
            pairs.reserve(entry.second.size());
            for (auto &field_pair : entry.second) {
                pairs.push_back(make_pair(field_pair.first, move(field_pair.second)));
            }
            kfv_entry.fieldValues = makeFieldValueArray(std::move(pairs));

            table_entries.push_back(kfv_entry);
        }

        // Convert to SWSSKeyOpFieldValuesArray
        SWSSKeyOpFieldValues *data = new SWSSKeyOpFieldValues[table_entries.size()];
        for (size_t i = 0; i < table_entries.size(); i++) {
            data[i] = table_entries[i];
        }

        SWSSKeyOpFieldValuesArray out;
        out.len = (uint64_t)table_entries.size();
        out.data = data;
        *outTable = out;
    });
}

SWSSResult SWSSConfigDBConnector_set_entry(SWSSConfigDBConnector configDb, const char *table, const char *key, const SWSSFieldValueArray *data) {
    SWSSTry({
        // Convert SWSSFieldValueArray to map<string, string>
        map<string, string> data_map;
        auto field_values = takeFieldValueArray(*data);
        for (auto &fv : field_values) {
            data_map[fv.first] = fv.second;
        }

        ((ConfigDBConnector_Native *)configDb)->set_entry(string(table), string(key), data_map);
    });
}

SWSSResult SWSSConfigDBConnector_mod_entry(SWSSConfigDBConnector configDb, const char *table, const char *key, const SWSSFieldValueArray *data) {
    SWSSTry({
        // Convert SWSSFieldValueArray to map<string, string>
        map<string, string> data_map;
        auto field_values = takeFieldValueArray(*data);
        for (auto &fv : field_values) {
            data_map[fv.first] = fv.second;
        }

        ((ConfigDBConnector_Native *)configDb)->mod_entry(string(table), string(key), data_map);
    });
}

SWSSResult SWSSConfigDBConnector_delete_table(SWSSConfigDBConnector configDb, const char *table) {
    SWSSTry(((ConfigDBConnector_Native *)configDb)->delete_table(string(table)));
}

SWSSResult SWSSConfigDBConnector_get_redis_client(SWSSConfigDBConnector configDb, const char *db_name, SWSSDBConnector *outDbConnector) {
    SWSSTry({
        // Get the Redis client from the ConfigDBConnector's parent SonicV2Connector_Native
        DBConnector& redis_client = ((ConfigDBConnector_Native *)configDb)->get_redis_client(string(db_name));

        // Return a pointer to the existing DBConnector
        // Note: This returns a reference to the existing connector, not a new one
        *outDbConnector = (SWSSDBConnector)&redis_client;
    });
}

SWSSResult SWSSConfigDBConnector_get_config(SWSSConfigDBConnector configDb, SWSSConfigMap *outConfig) {
    SWSSTry({
        *outConfig = makeConfigMap(((ConfigDBConnector_Native *)configDb)->get_config());
    });
}

SWSSResult SWSSConfigDBPipeConnector_new(uint8_t use_unix_socket_path, const char *netns,
                                         SWSSConfigDBPipeConnector *outConfigDb) {
    if (outConfigDb != nullptr)
    {
        *outConfigDb = nullptr;
    }
    SWSSTry({
        if (outConfigDb == nullptr)
        {
            throw invalid_argument("CONFIG_DB pipe connector output is null");
        }
        string netns_str = netns ? string(netns) : "";
        *outConfigDb = new SWSSConfigDBPipeConnectorOpaque(
            use_unix_socket_path != 0, netns_str.c_str());
    });
}

SWSSResult SWSSConfigDBPipeConnector_free(SWSSConfigDBPipeConnector configDb) {
    SWSSTry(delete configDb);
}

SWSSResult SWSSConfigDBPipeConnector_connect(SWSSConfigDBPipeConnector configDb,
                                             uint8_t wait_for_init, uint8_t retry_on) {
    SWSSTry({
        if (configDb == nullptr)
        {
            throw invalid_argument("CONFIG_DB pipe connector is null");
        }
        configDb->connector.connect(wait_for_init != 0, retry_on != 0);
    });
}

SWSSResult SWSSConfigDBPipeConnector_mod_config(SWSSConfigDBPipeConnector configDb,
                                                const SWSSConfigMap *config) {
    SWSSTry({
        if (configDb == nullptr)
        {
            throw invalid_argument("CONFIG_DB pipe connector is null");
        }
        configDb->connector.mod_config(copyConfigMap(config));
    });
}

SWSSResult SWSSConfigDBPipeConnector_get_config(SWSSConfigDBPipeConnector configDb,
                                                SWSSConfigMap *outConfig) {
    SWSSTry({
        if (configDb == nullptr)
        {
            throw invalid_argument("CONFIG_DB pipe connector is null");
        }
        if (outConfig == nullptr)
        {
            throw invalid_argument("CONFIG_DB configuration output is null");
        }
        *outConfig = makeConfigMap(configDb->connector.get_config());
    });
}
