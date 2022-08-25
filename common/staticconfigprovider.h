#pragma once

#include <map>
#include <string>
#include <vector>
#include "common/table.h"
#include "common/dbconnector.h"

namespace swss {

class StaticConfigProvider
{
public:
    static StaticConfigProvider& Instance();

    void AppendConfigs(const std::string &table, const std::string &key, std::map<std::string, std::string>& values, DBConnector* cfgDbConnector);

    void AppendConfigs(const std::string &table, const std::string &key, std::vector<std::pair<std::string, std::string> > &values, DBConnector* cfgDbConnector);

    void AppendConfigs(const std::string &table, KeyOpFieldsValuesTuple &operation, DBConnector* cfgDbConnector);

    std::shared_ptr<std::string> GetConfig(const std::string &table, const std::string &key, std::string field, DBConnector* cfgDbConnector);

    std::map<std::string, std::string> GetConfigs(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    std::vector<std::string> GetKeys(const std::string &table, DBConnector* cfgDbConnector);

    bool TryRevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    bool TryDeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

private:
    StaticConfigProvider();
    ~StaticConfigProvider();

    std::unordered_map<std::string, std::string> GetStaticConfig(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    std::string getKeyName(const std::string &table, const std::string &key)
    {
        return table + m_tableSeparator + key;
    }

    bool ItemDeleted(const std::string &key, DBConnector& staticCfgDbConnector);

    void DeleteItem(const std::string &key, DBConnector& staticCfgDbConnector);

    void RevertItem(const std::string &key, DBConnector& staticCfgDbConnector);

    DBConnector& GetStaticCfgDBConnector(DBConnector* cfgDbConnector);

    std::map<std::string, DBConnector> m_staticCfgDBMap;
    std::string m_tableSeparator;
};

}
