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

TEST(PROFILE, ChoiceAndLeaflistDefaultValue)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");
    
    // load default values from container with signle list
    auto values = instance.GetDefaultValues("TEST_YANGE_DEFAULT_VALUE", "TEST_KEY");
    EXPECT_EQ(values.size(), 2);

    // default value define in choice
    EXPECT_EQ(values["interval"], "30");

    // default value define in leaflist
    EXPECT_EQ(values["domain-search"], "[\"test\"]");
}

TEST(PROFILE, GetDefaultValues)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");
    
    // load default values from container with signle list
    auto values = instance.GetDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "0");
    
    // load default values from container with multiple list
    values = instance.GetDefaultValues("VLAN_INTERFACE", "Vlan1000");
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "0");

    values = instance.GetDefaultValues("VLAN_INTERFACE", "Vlan1000|192.168.0.1/21");
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "1");
    
    // load default values from container without list
    values = instance.GetDefaultValues("NAT_GLOBAL", "Values");
    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values["nat_tcp_timeout"], "86400");
    
    // check not existing table and keys
    values = instance.GetDefaultValues("NAT_GLOBAL", "NOT_EXIST_KEY");
    EXPECT_EQ(values.size(), 0);

    values = instance.GetDefaultValues("NOT_EXIST_TABLE", "NOT_EXIST_KEY");
    EXPECT_EQ(values.size(), 0);
}

TEST(PROFILE, AppendDefaultValuesMap)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");

    // test AppendDefaultValues with map
    map<string, string> values;
    instance.AppendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "0");

    // append again with existing values
    values.clear();
    values["existingKey"] = "existingValue";
    instance.AppendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values["nat_zone"], "0");
    EXPECT_EQ(values["existingKey"], "existingValue");

    // append again with existing overwrite config
    values.clear();
    values["nat_zone"] = "1";
    instance.AppendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values["nat_zone"], "1");
}

TEST(PROFILE, AppendDefaultValuesVector)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");

    // test AppendDefaultValues with vector
    vector<pair<string, string>> values;
    instance.AppendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values[0].first, "nat_zone");
    EXPECT_EQ(values[0].second, "0");

    // append again with existing values
    values.clear();
    values.push_back(pair<string, string>("existingKey", "existingValue"));
    instance.AppendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0].first, "existingKey");
    EXPECT_EQ(values[0].second, "existingValue");
    EXPECT_EQ(values[1].first, "nat_zone");
    EXPECT_EQ(values[1].second, "0");

    // append again with existing overwrite config
    values.clear();
    values.push_back(pair<string, string>("nat_zone", "1"));
    instance.AppendDefaultValues(profile_table, profile_key, values);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(values[0].first, "nat_zone");
    EXPECT_EQ(values[0].second, "1");
}

TEST(PROFILE, AppendDefaultValuesKeyOpFieldsValuesTuple)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang");

    // test AppendDefaultValues with vector
    KeyOpFieldsValuesTuple values;
    
    // test DEL command
    kfvOp(values) = DEL_COMMAND;
    kfvKey(values) = profile_key;
    auto& fieldValues = kfvFieldsValues(values);
    instance.AppendDefaultValues(profile_table, values);
    EXPECT_EQ(fieldValues.size(), 0);
    
    // test SET command
    kfvOp(values) = SET_COMMAND;
    instance.AppendDefaultValues(profile_table, values);
    EXPECT_EQ(fieldValues.size(), 1);
    EXPECT_EQ(fieldValues[0].first, "nat_zone");
    EXPECT_EQ(fieldValues[0].second, "0");

    // append again with existing values
    fieldValues.clear();
    fieldValues.push_back(pair<string, string>("existingKey", "existingValue"));
    instance.AppendDefaultValues(profile_table, values);
    EXPECT_EQ(fieldValues.size(), 2);
    EXPECT_EQ(fieldValues[0].first, "existingKey");
    EXPECT_EQ(fieldValues[0].second, "existingValue");
    EXPECT_EQ(fieldValues[1].first, "nat_zone");
    EXPECT_EQ(fieldValues[1].second, "0");

    // append again with existing overwrite config
    fieldValues.clear();
    fieldValues.push_back(pair<string, string>("nat_zone", "1"));
    instance.AppendDefaultValues(profile_table, values);
    EXPECT_EQ(fieldValues.size(), 1);
    EXPECT_EQ(fieldValues[0].first, "nat_zone");
    EXPECT_EQ(fieldValues[0].second, "1");
}

TEST(PROFILE, LoadYangModelWithMissingReference)
{
    DefaultValueProvider instance;
    instance.Initialize("./tests/yang-missing-ref");
    
    // when reference missing, load default value will failed
    auto values = instance.GetDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 0);
}

