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

string runCli(int argc, char** argv, int expected_exit_code = 0)
{
    optind = 0;
    testing::internal::CaptureStdout();
    int exit_code = sonic_db_cli(argc, argv);
    auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(expected_exit_code, exit_code);
    return output;
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

TEST(sonic_db_cli, test_cli_hscan_commands)
{
    char *args[6];
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";

    // clear database
    args[2] = "FLUSHDB";
    auto output = runCli(3, args);
    EXPECT_EQ("True\n", output);

    // hset to test DB
    args[2] = "HSET";
    args[3] = "testkey";
    args[4] = "testfield";
    args[5] = "testvalue";
    output = runCli(6, args);
    EXPECT_EQ("1\n", output);
    
    // hgetall from test db
    args[2] = "HGETALL";
    args[3] = "testkey";
    output = runCli(4, args);
    EXPECT_EQ("{'testfield': 'testvalue'}\n", output);
    
    // hscan from test db
    args[2] = "HSCAN";
    args[3] = "testkey";
    args[4] = "0";
    output = runCli(5, args);
    EXPECT_EQ("(0, {'testfield': 'testvalue'})\n", output);

    // hgetall from test db
    args[2] = "HGETALL";
    args[3] = "notexistkey";
    output = runCli(4, args);
    EXPECT_EQ("{}\n", output);
    
    // hscan from test db
    args[2] = "HSCAN";
    args[3] = "notexistkey";
    args[4] = "0";
    output = runCli(5, args);
    EXPECT_EQ("(0, {})\n", output);
}

TEST(sonic_db_cli, test_cli_pop_commands)
{
    char *args[10];
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";

    // clear database
    args[2] = "FLUSHDB";
    auto output = runCli(3, args);
    EXPECT_EQ("True\n", output);

    // rpush to test DB
    args[2] = "rpush";
    args[3] = "list1";
    args[4] = "a";
    args[5] = "b";
    args[6] = "c";
    args[7] = "d";
    args[8] = "e";
    output = runCli(9, args);
    EXPECT_EQ("5\n", output);
    
    // pop from test db
    args[2] = "blpop";
    args[3] = "list1";
    args[4] = "list2";
    args[5] = "0";
    output = runCli(6, args);
    EXPECT_EQ("('list1', 'a')\n", output);
}

TEST(sonic_db_cli, test_cli_expire_commands)
{
    char *args[10];
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";

    // clear database
    args[2] = "FLUSHDB";
    auto output = runCli(3, args);
    EXPECT_EQ("True\n", output);

    // rpush to test DB
    args[2] = "set";
    args[3] = "testkey";
    args[4] = "test";
    output = runCli(5, args);
    EXPECT_EQ("True\n", output);
    
    // pop from test db
    args[2] = "expire";
    args[3] = "testkey";
    args[4] = "10";
    output = runCli(5, args);
    EXPECT_EQ("True\n", output);
}

TEST(sonic_db_cli, test_cli_sscan_commands)
{
    char *args[9];
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";

    // clear database
    args[2] = "FLUSHDB";
    auto output = runCli(3, args);
    EXPECT_EQ("True\n", output);

    // create set to test DB
    args[2] = "sadd";
    args[3] = "myset";
    args[4] = "1";
    args[5] = "2";
    args[6] = "foobar";
    output = runCli(7, args);
    EXPECT_EQ("3\n", output);

    // sscan from test db
    args[2] = "sscan";
    args[3] = "myset";
    args[4] = "0";
    args[5] = "match";
    args[6] = "f*";
    output = runCli(7, args);
    EXPECT_EQ("(0, ['foobar'])\n", output);

    // create set to test DB
    args[2] = "sadd";
    args[3] = "myset2";
    args[4] = "1";
    args[5] = "2";
    output = runCli(6, args);
    EXPECT_EQ("2\n", output);

    // sscan from test db
    args[2] = "sscan";
    args[3] = "myset2";
    args[4] = "0";
    args[5] = "match";
    args[6] = "f*";
    output = runCli(7, args);
    EXPECT_EQ("(0, [])\n", output);
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
    EXPECT_EQ("True\n", output);
    
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
    EXPECT_EQ("True\n", output);
    
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
    EXPECT_EQ("True\n", output);
    
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
