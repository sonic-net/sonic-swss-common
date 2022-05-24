#include "gtest/gtest.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <getopt.h>
#include "common/dbconnector.h"
#include "sonic-db-cli/sonic-db-cli.h"

using namespace swss;
using namespace std;

const string config_file = "./tests/redis_multi_db_ut_config/database_config.json";
const string global_config_file = "./tests/redis_multi_db_ut_config/database_global.json";

int sonic_db_cli(int argc, char** argv)
{
    auto initializeGlobalConfig = []()
    {
        if (!SonicDBConfig::isGlobalInit())
        {
            SonicDBConfig::initializeGlobalConfig(global_config_file);
        }
    };

    auto initializeConfig = []()
    {
        if (!SonicDBConfig::isInit())
        {
            SonicDBConfig::initialize(config_file);
        }
    };

    return sonic_db_cli(
                    argc,
                    argv,
                    initializeGlobalConfig,
                    initializeConfig);
}

string readFileContent(string file_name)
{
    ifstream help_output_file(file_name);
    stringstream buffer;
    buffer << help_output_file.rdbuf();
    return buffer.str();
}

string runCli(int argc, char** argv)
{
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, sonic_db_cli(argc, argv));
    auto output = testing::internal::GetCapturedStdout();
    return output;
}

TEST(sonic_db_cli, test_cli_help)
{
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "-h";

    auto output = runCli(2, args);
    auto expected_output = readFileContent("./tests/cli_test_data/cli_help_output.txt");
    EXPECT_EQ(expected_output, output);
}

TEST(sonic_db_cli, test_cli_ping_cmd)
{
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "PING";

    auto output = runCli(2, args);
    EXPECT_EQ("PONG\n", output);
}

TEST(sonic_db_cli, test_cli_save_cmd)
{
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "SAVE";

    auto output = runCli(2, args);
    EXPECT_EQ("OK\n", output);
}

TEST(sonic_db_cli, test_cli_flush_cmd)
{
    char *args[2];
    args[0] = "sonic-db-cli";
    args[1] = "FLUSHALL";

    auto output = runCli(2, args);
    EXPECT_EQ("OK\n", output);
}

TEST(sonic_db_cli, test_cli_run_cmd)
{
    char *args[5];
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";
    
    // set key to test DB
    args[2] = "SET";
    args[3] = "testkey";
    args[4] = "testvalue";
    auto output = runCli(5, args);
    EXPECT_EQ("OK\n", output);
    
    // get key from test db
    args[2] = "GET";
    args[3] = "testkey";
    output = runCli(4, args);
    EXPECT_EQ("testvalue\n", output);
    
    // get keys from test db
    args[2] = "keys";
    args[3] = "*";
    output = runCli(4, args);
    EXPECT_EQ("testkey\n", output);
}

TEST(sonic_db_cli, test_cli_multi_ns_cmd)
{
    char *args[7];
    args[0] = "sonic-db-cli";
    args[1] = "-n";
    args[2] = "asic0";
    args[3] = "TEST_DB";
    
    // set key to test DB
    args[4] = "SET";
    args[5] = "testkey";
    args[6] = "testvalue";
    auto output = runCli(7, args);
    EXPECT_EQ("OK\n", output);
    
    // get key from test db
    args[4] = "GET";
    args[5] = "testkey";
    output = runCli(6, args);
    EXPECT_EQ("testvalue\n", output);
    
    // get keys from test db
    args[4] = "keys";
    args[5] = "*";
    output = runCli(6, args);
    EXPECT_EQ("testkey\n", output);
}

TEST(sonic_db_cli, test_cli_unix_socket_cmd)
{
    char *args[8];
    args[0] = "sonic-db-cli";
    args[1] = "-s";
    args[2] = "-n";
    args[3] = "asic0";
    args[4] = "TEST_DB";
    
    // set key to test DB
    args[5] = "SET";
    args[6] = "testkey";
    args[7] = "testvalue";
    auto output = runCli(8, args);
    EXPECT_EQ("OK\n", output);
    
    // get key from test db
    args[5] = "GET";
    args[6] = "testkey";
    output = runCli(7, args);
    EXPECT_EQ("testvalue\n", output);
}

TEST(sonic_db_cli, test_cli_eval_cmd)
{
    char *args[11];
    args[0] = "sonic-db-cli";
    args[1] = "-n";
    args[2] = "asic0";
    args[3] = "TEST_DB";
    
    // run eval command: EVAL "return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}" 2 k1 k2 v1 v2
    args[4] = "EVAL";
    args[5] = "return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}";
    args[6] = "2";
    args[7] = "k1";
    args[8] = "k2";
    args[9] = "v1";
    args[10] = "v2";
    auto output = runCli(11, args);
    EXPECT_EQ("k1\nk2\nv1\nv2\n", output);
}

void flushDB(char* ns, char* database)
{
    char *args[5];
    args[0] = "sonic-db-cli";
    args[1] = "-n";
    args[2] = ns;
    args[3] = database;
    args[4] = "FLUSHDB";
    optind = 0;
    sonic_db_cli(5, args);
}

void generateTestData(char* ns, char* database)
{
    flushDB(ns, database);
    char *args[7];
    args[0] = "sonic-db-cli";
    args[1] = "-n";
    args[2] = ns;
    args[3] = database;
    
    args[4] = "EVAL";
    args[5] = "local i=0 while (i<100000) do i=i+1 redis.call('SET', i, i) end";
    args[6] = "0";
    optind = 0;
    sonic_db_cli(7, args);
}

TEST(sonic_db_cli, test_parallel_cmd) {
    char *args[7];
    args[0] = "sonic-db-cli";
    args[1] = "-n";
    args[2] = "asic0";
    args[4] = "SAVE";

    auto db_names = swss::SonicDBConfig::getDbList("asic0");
    for (auto& db_name : db_names)
    {
        generateTestData("asic0", const_cast<char*>(db_name.c_str()));
    }
    db_names = swss::SonicDBConfig::getDbList("asic1");
    for (auto& db_name : db_names)
    {
        generateTestData("asic1", const_cast<char*>(db_name.c_str()));
    }

    // save 2 DBs and get save DB performance
    auto begin_time = clock();
    db_names = swss::SonicDBConfig::getDbList("asic0");
    args[2] = "asic0";
    for (auto& db_name : db_names)
    {
        args[3] = const_cast<char*>(db_name.c_str());
        optind = 0;
        sonic_db_cli(5, args);
    }

    db_names = swss::SonicDBConfig::getDbList("asic1");
    args[2] = "asic1";
    for (auto& db_name : db_names)
    {
        args[3] = const_cast<char*>(db_name.c_str());
        optind = 0;
        sonic_db_cli(5, args);
    }

    auto sequential_time = float( clock () - begin_time );

    // prepare data
    db_names = swss::SonicDBConfig::getDbList("asic0");
    for (auto& db_name : db_names)
    {
        generateTestData("asic0", const_cast<char*>(db_name.c_str()));
    }
    db_names = swss::SonicDBConfig::getDbList("asic1");
    for (auto& db_name : db_names)
    {
        generateTestData("asic0", const_cast<char*>(db_name.c_str()));
    }

    // save 2 DBs in parallel, and get save DB performance
    begin_time = clock();

    args[1] = "SAVE";
    optind = 0;
    sonic_db_cli(2, args);

    auto parallen_time = float( clock () - begin_time );
    EXPECT_TRUE(parallen_time < sequential_time);
}