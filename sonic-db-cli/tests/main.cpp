#include "gtest/gtest.h"
#include <swss/dbconnector.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include "sonic-db-cli.h"

std::string db_file = "./database_config.json";
std::string global_db_file = "./database_global.json";

#define TEST_DB  "APPL_DB"
#define TEST_NAMESPACE  "asic0"
#define INVALID_NAMESPACE  "invalid"

class SwsscommonEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // load local config file
        //SonicDBConfig::initialize(db_file);
        //EXPECT_TRUE(SonicDBConfig::isInit());

        // load local global file
        swss::SonicDBConfig::initializeGlobalConfig(global_db_file);
        EXPECT_TRUE(swss::SonicDBConfig::isGlobalInit());
    }
};

std::string readFileContent(std::string file_name) {
    std::ifstream help_output_file(file_name);
    std::stringstream buffer;
    buffer << help_output_file.rdbuf();
    return buffer.str();
}

TEST(sonic_db_cli, test_cli_help) {
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "-h";
    
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(1, args));
    auto output = testing::internal::GetCapturedStdout();
    auto expected_output = readFileContent("help_output.txt");
    EXPECT_EQ(expected_output, output);
}

TEST(sonic_db_cli, test_cli_default_ns_run_cmd) {
    char *args[5];
    args[0] = "sonic-db-cli";
    args[1] = "APPL_DB";
    args[2] = "HGET";
    args[3] = "VLAN_TABLE:Vlan10";
    args[4] = "mtu";
    
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(4, args));
    auto output = testing::internal::GetCapturedStdout();
    std::cout << output <<std::endl;
    auto expected_output = readFileContent("help_output.txt");
    EXPECT_EQ(expected_output, output);
}

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    // Registers a global test environment, and verifies that the
    // registration function returns its argument.
    SwsscommonEnvironment* const env = new SwsscommonEnvironment;
    testing::AddGlobalTestEnvironment(env);
    return RUN_ALL_TESTS();
}