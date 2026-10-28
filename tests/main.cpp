#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/c-api/util.h"
#include <iostream>

using namespace std;
using namespace swss;

string existing_file = "./tests/redis_multi_db_ut_config/database_config.json";
string nonexisting_file = "./tests/redis_multi_db_ut_config/database_config_nonexisting.json";
string global_existing_file = "./tests/redis_multi_db_ut_config/database_global.json";
string global_with_invalid_include = "./tests/redis_multi_db_ut_config/database_global_with_invalid_include.json";
#define TEST_DB  "APPL_DB"
#define TEST_NAMESPACE  "asic0"
#define INVALID_NAMESPACE  "invalid"

class SwsscommonEnvironment : public ::testing::Environment {
public:
    // Override this to define how to set up the environment.
    void SetUp() override {
        // by default , init should be false
        cout<<"Default : isInit = "<<SonicDBConfig::isInit()<<endl;
        EXPECT_FALSE(SonicDBConfig::isInit());

        // load nonexisting file, should throw exception with NO file existing
        cout<<"INIT: loading nonexisting db config file"<<endl;
        EXPECT_THROW(SonicDBConfig::initialize(nonexisting_file), std::runtime_error);
        EXPECT_FALSE(SonicDBConfig::isInit());

        // load local config file, init should be true
        SonicDBConfig::initialize(existing_file);
        cout<<"INIT: load local db config file, isInit = "<<SonicDBConfig::isInit()<<endl;
        EXPECT_TRUE(SonicDBConfig::isInit());

        // Test the database_global.json file
        // by default , global_init should be false
        cout<<"Default : isGlobalInit = "<<SonicDBConfig::isGlobalInit()<<endl;
        EXPECT_FALSE(SonicDBConfig::isGlobalInit());

        // Call an API which actually needs the data populated by SonicDBConfig::initializeGlobalConfig
        cout<<"INIT: Invoking SonicDBConfig::getDbId(APPL_DB, asic0)"<<endl;
        EXPECT_THROW(SonicDBConfig::getDbId(TEST_DB, TEST_NAMESPACE), std::runtime_error);

        // Test the global SonicDBConfig::initializeGlobalConfig with non-existing include
        SonicDBConfig::initializeGlobalConfig(global_with_invalid_include, true);
        cout<<"INIT: load global db config file with invalid include, isGlobalInit = "<<SonicDBConfig::isGlobalInit()<<endl;
        EXPECT_TRUE(SonicDBConfig::isGlobalInit());
        vector<SonicDBKey> db_keys = SonicDBConfig::getDbKeys();

        // Extract containerName from SonicDBKey in db_keys and store in vector<string>
        vector<string> cn_actual;
        for (const auto& key : db_keys) {
            cn_actual.push_back(key.containerName);
        }

        sort (cn_actual.begin(), cn_actual.end());
        vector<string> cn_expected = {"", "dpu0", "dpu2"};
        // verify the non-existent include is skipped
        EXPECT_EQ(cn_actual.size(), cn_expected.size());
        EXPECT_TRUE(std::equal(cn_expected.begin(), cn_expected.end(), cn_actual.begin()));
        // reset SonicDBConfig, init should be false
        SonicDBConfig::reset();
        cout<<"RESET: isInit = "<<SonicDBConfig::isInit()<<endl;
        EXPECT_FALSE(SonicDBConfig::isInit());
        EXPECT_FALSE(SonicDBConfig::isGlobalInit());

        // load local global file, init should be true
        SonicDBConfig::initializeGlobalConfig(global_existing_file);
        cout<<"INIT: load global db config file, isInit = "<<SonicDBConfig::isGlobalInit()<<endl;
        EXPECT_TRUE(SonicDBConfig::isGlobalInit());

        // Call an API with wrong namespace passed
        cout<<"INIT: Invoking SonicDBConfig::getDbId(APPL_DB, invalid)"<<endl;
        EXPECT_THROW(SonicDBConfig::getDbId(TEST_DB, INVALID_NAMESPACE), std::out_of_range);

        // reset SonicDBConfig, init should be false
        SonicDBConfig::reset();
        cout<<"RESET: isInit = "<<SonicDBConfig::isInit()<<endl;
        EXPECT_FALSE(SonicDBConfig::isInit());
        EXPECT_FALSE(SonicDBConfig::isGlobalInit());

        // reinitialize SonicDBConfig, init should be true
        SonicDBConfig::initialize(existing_file);
        cout<<"INIT: load local db config file, isInit = "<<SonicDBConfig::isInit()<<endl;
        EXPECT_TRUE(SonicDBConfig::isInit());
        SonicDBConfig::initializeGlobalConfig(global_existing_file);
        cout<<"INIT: load global db config file, isInit = "<<SonicDBConfig::isGlobalInit()<<endl;
        EXPECT_TRUE(SonicDBConfig::isGlobalInit());
    }
};

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    // Registers a global test environment, and verifies that the
    // registration function returns its argument.
    SwsscommonEnvironment* const env = new SwsscommonEnvironment;
    testing::AddGlobalTestEnvironment(env);
    return RUN_ALL_TESTS();
}
