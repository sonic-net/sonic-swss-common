#include "gtest/gtest.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include "sonic-db-cli/sonic-db-cli.h"

#define TEST_DB  "APPL_DB"
#define TEST_NAMESPACE  "asic0"
#define INVALID_NAMESPACE  "invalid"

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
    auto expected_output = readFileContent("cli_help_output.txt");
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
    auto expected_output = readFileContent("cli_help_output.txt");
    EXPECT_EQ(expected_output, output);
}