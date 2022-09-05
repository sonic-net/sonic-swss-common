#pragma once

#include <map>
#include <string>
#include <vector>
#include "common/table.h"
#include "common/dbconnector.h"
#include "common/converter.h"

namespace swss {

class StaticConfigProvider
{
public:
    static StaticConfigProvider& Instance();

    void AppendConfigs(const std::string &table, const std::string &key, std::map<std::string, std::string>& values, DBConnector* cfgDbConnector);

    void AppendConfigs(const std::string &table, const std::string &key, std::vector<std::pair<std::string, std::string> > &values, DBConnector* cfgDbConnector);

    void AppendConfigs(const std::string &table, KeyOpFieldsValuesTuple &operation, DBConnector* cfgDbConnector);

    std::shared_ptr<std::string> GetConfig(const std::string &table, const std::string &key, const std::string &field, DBConnector* cfgDbConnector);

    std::map<std::string, std::string> GetConfigs(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> GetConfigs(DBConnector* cfgDbConnector);

    std::vector<std::string> GetKeys(const std::string &table, DBConnector* cfgDbConnector);

    bool TryRevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    bool TryDeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

private:
    StaticConfigProvider();
    ~StaticConfigProvider();

    std::string getKeyName(const std::string &table, const std::string &key, DBConnector* dbConnector)
    {
        const auto separator = SonicDBConfig::getSeparator(dbConnector);
        // Profile DB follow Config DB: table name is case insensetive.
        return to_upper(table) + separator + key;
    }

    std::string getDeletedKeyName(const std::string &table, const std::string &key, DBConnector* dbConnector)
    {
        auto itemKey = to_upper(table) + "_" + key;
        return getKeyName(PROFILE_DELETE_TABLE, itemKey, dbConnector);
    }


    bool ItemDeleted(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    void DeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    void RevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    DBConnector& GetStaticCfgDBConnector(DBConnector* cfgDbConnector);

    std::map<std::string, DBConnector> m_staticCfgDBMap;
};

}
