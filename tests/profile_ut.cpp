#include "gtest/gtest.h"
#include "common/defaultvalueprovider.h"
#include "common/staticconfigprovider.h"
#include "common/dbconnector.h"
#include "common/configdb.h"

using namespace std;
using namespace swss;

string profile_table = "INTERFACE";
string profile_key = "TEST_INTERFACE";
string profile_field = "profile";
string profile_value = "value";

void clearDB(const string &dbName)
{
    DBConnector db(dbName, 0, true);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void initializeProfileDB()
{
    clearDB("PROFILE_DB");
    clearDB("CONFIG_DB");

    auto db = ConfigDBConnector_Native();
    db.db_connect("PROFILE_DB");
    map<string, string> profile_map = {
        { profile_field, profile_value }
    };
    db.set_entry(profile_table, profile_key, profile_map);

    auto item = db.get_entry(profile_table, profile_key);
    EXPECT_EQ(item[profile_field], profile_value);
}

TEST(PROFILE, GetConfigs)
{
    initializeProfileDB();

    DBConnector db("CONFIG_DB", 0, true);

    auto configs = StaticConfigProvider::Instance().GetConfigs(&db);
    EXPECT_EQ(configs[profile_table][profile_key][profile_field], profile_value);

    auto item = StaticConfigProvider::Instance().GetConfigs(profile_table, profile_key, &db);
    EXPECT_EQ(item[profile_field], profile_value);

    auto valueptr = StaticConfigProvider::Instance().GetConfig(profile_table, profile_key, profile_field, &db);
    EXPECT_EQ(*valueptr, profile_value);

    auto keys = StaticConfigProvider::Instance().GetKeys(profile_table, &db);
    EXPECT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], profile_key);
}

TEST(PROFILE, DeleteAndRevertProfile)
{
    initializeProfileDB();

    DBConnector db("CONFIG_DB", 0, true);
    auto connector = ConfigDBConnector_Native();
    connector.connect(false);

    // test delete
    bool result = StaticConfigProvider::Instance().TryDeleteItem(profile_table, profile_key, &db);
    EXPECT_EQ(result, true);

    auto deletedItems = connector.get_keys(PROFILE_DELETE_TABLE, false);
    EXPECT_EQ(deletedItems.size(), 1);

    auto configs = StaticConfigProvider::Instance().GetConfigs(&db);
    EXPECT_EQ(configs[profile_table].find(profile_key), configs[profile_table].end());

    auto item = StaticConfigProvider::Instance().GetConfigs(profile_table, profile_key, &db);
    EXPECT_EQ(item.size(), 0);

    auto valueptr = StaticConfigProvider::Instance().GetConfig(profile_table, profile_key, profile_field, &db);
    EXPECT_EQ(valueptr, nullptr);

    auto keys = StaticConfigProvider::Instance().GetKeys(profile_table, &db);
    EXPECT_EQ(keys.size(), 0);

    // test revert
    result = StaticConfigProvider::Instance().TryRevertItem(profile_table, profile_key, &db);
    EXPECT_EQ(result, true);

    deletedItems = connector.get_keys(PROFILE_DELETE_TABLE, false);
    EXPECT_EQ(deletedItems.size(), 0);

    configs = StaticConfigProvider::Instance().GetConfigs(&db);
    EXPECT_EQ(configs[profile_table][profile_key][profile_field], profile_value);

    item = StaticConfigProvider::Instance().GetConfigs(profile_table, profile_key, &db);
    EXPECT_EQ(item[profile_field], profile_value);

    valueptr = StaticConfigProvider::Instance().GetConfig(profile_table, profile_key, profile_field, &db);
    EXPECT_EQ(*valueptr, profile_value);

    keys = StaticConfigProvider::Instance().GetKeys(profile_table, &db);
    EXPECT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], profile_key);
}

TEST(PROFILE, GetDefaultValues)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");
    
    auto values = instance.GetDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "0");
}

TEST(PROFILE, LoadYangModelWithMissingReference)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang-missing-ref");
    
    // when reference missing, load default value will failed
    auto values = instance.GetDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 0);
}

