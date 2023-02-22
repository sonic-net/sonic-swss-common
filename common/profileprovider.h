#pragma once

#include <map>
#include <string>
#include <vector>
#include "table.h"
#include "dbconnector.h"
#include "converter.h"

namespace swss {

const std::string DELETED_KEY_SEPARATOR = "_";

class ProfileProvider
{
public:
    static ProfileProvider& instance();

    bool appendConfigs(const std::string &table, const std::string &key, std::vector<std::pair<std::string, std::string> > &values, DBConnector* cfgDbConnector);

    std::shared_ptr<std::string> getConfig(const std::string &table, const std::string &key, const std::string &field, DBConnector* cfgDbConnector);

    std::map<std::string, std::string> getConfigs(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> getConfigs(DBConnector* cfgDbConnector);

    std::vector<std::string> getKeys(const std::string &table, DBConnector* cfgDbConnector);

    bool tryRevertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    bool tryDeleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    std::string getDeletedKeyName(const std::string &table, const std::string &key, DBConnector* dbConnector)
    {
        auto itemKey = to_upper(table) + DELETED_KEY_SEPARATOR + key;
        return getKeyName(PROFILE_DELETE_TABLE, itemKey, dbConnector);
    }

private:
    ProfileProvider();
    ~ProfileProvider();

    std::string getKeyName(const std::string &table, const std::string &key, DBConnector* dbConnector)
    {
        const auto separator = SonicDBConfig::getSeparator(dbConnector);
        // Profile DB follow Config DB: table name is case insensetive.
        return to_upper(table) + separator + key;
    }

    bool itemDeleted(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    void deleteItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    void revertItem(const std::string &table, const std::string &key, DBConnector* cfgDbConnector);

    DBConnector& getStaticCfgDBConnector(DBConnector* cfgDbConnector);

    std::map<std::string, DBConnector> m_staticCfgDBMap;
};

}
