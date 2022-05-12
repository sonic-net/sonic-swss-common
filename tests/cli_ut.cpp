#include "gtest/gtest.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <getopt.h>
#include "sonic-db-cli/sonic-db-cli.h"

#define TEST_DB  "STATE_DB2"
#define TEST_NAMESPACE  "asic0"
#define INVALID_NAMESPACE  "invalid"

const std::string config_file = "./tests/redis_multi_db_ut_config/database_config.json";
const std::string global_config_file = "./tests/redis_multi_db_ut_config/database_global.json";

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

    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(config_file, global_config_file, 2, args));
    auto output = testing::internal::GetCapturedStdout();
    auto expected_output = readFileContent("./tests/cli_test_data/cli_help_output.txt");
    EXPECT_EQ(expected_output, output);
}

TEST(sonic_db_cli, test_cli_ping_cmd) {
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "PING";

    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(config_file, global_config_file, 2, args));
    auto output = testing::internal::GetCapturedStdout();
    std::cout << output <<std::endl;
    EXPECT_EQ("PONG\n", output);
}

TEST(sonic_db_cli, test_cli_save_cmd) {
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "SAVE";

    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(config_file, global_config_file, 2, args));
    auto output = testing::internal::GetCapturedStdout();
    std::cout << output <<std::endl;
    EXPECT_EQ("OK\n", output);
}

TEST(sonic_db_cli, test_cli_flush_cmd) {
    char *args[5];
    args[0] = "sonic-db-cli";
    args[1] = "FLUSHALL";

    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(config_file, global_config_file, 2, args));
    auto output = testing::internal::GetCapturedStdout();
    std::cout << output <<std::endl;
    EXPECT_EQ("OK\n", output);
}