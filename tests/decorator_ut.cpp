#include "gtest/gtest.h"
#include "common/defaultvalueprovider.h"
#include "common/profileprovider.h"
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

TEST(DECORATOR, ChoiceAndLeaflistDefaultValue)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");
    
    // load default values from container with signle list
    auto values = instance.getDefaultValues("TEST_YANGE_DEFAULT_VALUE", "TEST_KEY");
    EXPECT_EQ(values.size(), 2);

    // default value define in choice
    EXPECT_EQ(values["interval"], "30");

    // default value define in leaflist
    EXPECT_EQ(values["domain-search"], "[\"test\"]");
}

TEST(DECORATOR, GetDefaultValues)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");
    
    // load default values from container with signle list
    auto values = instance.getDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "0");
    
    // load default values from container with multiple list
    values = instance.getDefaultValues("VLAN_INTERFACE", "Vlan1000");
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "0");

    values = instance.getDefaultValues("VLAN_INTERFACE", "Vlan1000|192.168.0.1/21");
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "1");
    
    // load default values from container without list
    values = instance.getDefaultValues("NAT_GLOBAL", "Values");
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values["nat_tcp_timeout"], "86400");
    
    // check not existing table and keys
    values = instance.getDefaultValues("NAT_GLOBAL", "NOT_EXIST_KEY");
    EXPECT_EQ(values.size(), 0);

    values = instance.getDefaultValues("NOT_EXIST_TABLE", "NOT_EXIST_KEY");
    EXPECT_EQ(values.size(), 0);
}

TEST(DECORATOR, AppendDefaultValuesVector)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");

    // test AppendDefaultValues with vector
    vector<pair<string, string>> values;
    instance.appendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values[0].first, "nat_zone");
    EXPECT_EQ(values[0].second, "0");

    // append again with existing values
    values.clear();
    values.push_back(pair<string, string>("existingKey", "existingValue"));
    instance.appendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0].first, "existingKey");
    EXPECT_EQ(values[0].second, "existingValue");
    EXPECT_EQ(values[1].first, "nat_zone");
    EXPECT_EQ(values[1].second, "0");

    // append again with existing overwrite config
    values.clear();
    values.push_back(pair<string, string>("nat_zone", "1"));
    instance.appendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values[0].first, "nat_zone");
    EXPECT_EQ(values[0].second, "1");
}

TEST(DECORATOR, LoadYangModelWithMissingReference)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang-missing-ref");
    
    // when reference missing, load default value will failed
    auto values = instance.getDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 0);
}

