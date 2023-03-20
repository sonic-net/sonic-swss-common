#include "gtest/gtest.h"
#include "common/profileprovider.h"
#include "common/dbconnector.h"
#include "common/configdb.h"

using namespace std;
using namespace swss;

static string profile_table = "INTERFACE";
static string profile_key = "TEST_INTERFACE";
static string profile_field = "profile";
static string profile_value = "value";

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

TEST(DECORATOR, GetConfigs)
{
    initializeProfileDB();

    DBConnector db("CONFIG_DB", 0, true);

    auto configs = ProfileProvider::instance().getConfigs(&db);
    EXPECT_EQ(configs[profile_table][profile_key][profile_field], profile_value);

    auto item = ProfileProvider::instance().getConfigs(profile_table, profile_key, &db);
    EXPECT_EQ(item[profile_field], profile_value);

    auto valueptr = ProfileProvider::instance().getConfig(profile_table, profile_key, profile_field, &db);
    EXPECT_EQ(*valueptr, profile_value);

    auto keys = ProfileProvider::instance().getKeys(profile_table, &db);
    EXPECT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], profile_key);
}

TEST(DECORATOR, DeleteAndRevertProfile)
{
    initializeProfileDB();

    DBConnector db("CONFIG_DB", 0, true);
    auto connector = ConfigDBConnector_Native();
    connector.connect(false);

    // test delete
    bool result = ProfileProvider::instance().tryDeleteItem(profile_table, profile_key, &db);
    EXPECT_EQ(result, true);

    auto deletedItems = connector.get_keys(PROFILE_DELETE_TABLE, false);
    EXPECT_EQ(deletedItems.size(), 1);

    auto configs = ProfileProvider::instance().getConfigs(&db);
    EXPECT_EQ(configs[profile_table].find(profile_key), configs[profile_table].end());

    auto item = ProfileProvider::instance().getConfigs(profile_table, profile_key, &db);
    EXPECT_EQ(item.size(), 0);

    auto valueptr = ProfileProvider::instance().getConfig(profile_table, profile_key, profile_field, &db);
    EXPECT_EQ(valueptr, nullptr);

    auto keys = ProfileProvider::instance().getKeys(profile_table, &db);
    EXPECT_EQ(keys.size(), 0);

    // test revert
    result = ProfileProvider::instance().tryRevertItem(profile_table, profile_key, &db);
    EXPECT_EQ(result, true);

    deletedItems = connector.get_keys(PROFILE_DELETE_TABLE, false);
    EXPECT_EQ(deletedItems.size(), 0);

    configs = ProfileProvider::instance().getConfigs(&db);
    EXPECT_EQ(configs[profile_table][profile_key][profile_field], profile_value);

    item = ProfileProvider::instance().getConfigs(profile_table, profile_key, &db);
    EXPECT_EQ(item[profile_field], profile_value);

    valueptr = ProfileProvider::instance().getConfig(profile_table, profile_key, profile_field, &db);
    EXPECT_EQ(*valueptr, profile_value);

    keys = ProfileProvider::instance().getKeys(profile_table, &db);
    EXPECT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], profile_key);
}
