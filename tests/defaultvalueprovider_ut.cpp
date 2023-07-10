#include "gtest/gtest.h"
#include "common/defaultvalueprovider.h"

using namespace std;
using namespace swss;

static string profile_table = "INTERFACE";
static string profile_key = "TEST_INTERFACE";

class MockDefaultValueProvider : public DefaultValueProvider
{
public:
    void MockInitialize(const char* modulePath)
    {
        this->Initialize(modulePath);
    }
};

TEST(DECORATOR, ChoiceAndLeaflistDefaultValue)
{
    MockDefaultValueProvider instance;
    instance.MockInitialize("./tests/yang");
    
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
    MockDefaultValueProvider instance;
    instance.MockInitialize("./tests/yang");
    
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
    MockDefaultValueProvider instance;
    instance.MockInitialize("./tests/yang");

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
    MockDefaultValueProvider instance;
    instance.MockInitialize("./tests/yang-missing-ref");
    
    // when reference missing, load default value will failed
    auto values = instance.getDefaultValues(profile_table, profile_key);
    EXPECT_EQ(values.size(), 0);
}

