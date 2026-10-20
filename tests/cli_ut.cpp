#include "gtest/gtest.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <getopt.h>
#include "common/dbconnector.h"
#include "sonic-db-cli/sonic-db-cli.h"
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using namespace swss;
using namespace std;

const string not_exist_config_file = "./tests/redis_multi_db_ut_config/database_config_not_exist.json";
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

    // hset to test DB
    args[2] = "HSET";
    args[3] = "testkey";
    args[4] = "testfield";
    args[5] = "{'value': 'with qute'}";
    output = runCli(6, args);
    EXPECT_EQ("0\n", output);

    // hgetall from test db
    args[2] = "HGETALL";
    args[3] = "testkey";
    output = runCli(4, args);
    EXPECT_EQ("{'testfield': \"{'value': 'with qute'}\"}\n", output);
}

TEST(sonic_db_cli, test_cli_json_output)
{
    char *args[7];
    args[0] = "sonic-db-cli";
    args[1] = "-j";
    args[2] = "TEST_DB";

    // clear database, status reply becomes a JSON string
    args[3] = "FLUSHDB";
    auto output = runCli(4, args);
    EXPECT_EQ("\"OK\"\n", output);

    // hset to test DB, integer reply becomes a JSON number
    args[3] = "HSET";
    args[4] = "testkey";
    args[5] = "testfield";
    args[6] = "testvalue";
    output = runCli(7, args);
    EXPECT_EQ("1\n", output);

    // hgetall from test db becomes a JSON object
    args[3] = "HGETALL";
    args[4] = "testkey";
    output = runCli(5, args);
    EXPECT_EQ("{\"testfield\":\"testvalue\"}\n", output);

    // hgetall of a missing key becomes an empty JSON object
    args[3] = "HGETALL";
    args[4] = "notexistkey";
    output = runCli(5, args);
    EXPECT_EQ("{}\n", output);

    // hscan from test db, cursor is a JSON string and fields a JSON object
    args[3] = "HSCAN";
    args[4] = "testkey";
    args[5] = "0";
    output = runCli(6, args);
    EXPECT_EQ("[\"0\",{\"testfield\":\"testvalue\"}]\n", output);

    // values with quote characters are escaped
    args[3] = "HSET";
    args[4] = "quotekey";
    args[5] = "testfield";
    args[6] = "va'l\"ue";
    output = runCli(7, args);
    EXPECT_EQ("1\n", output);

    args[3] = "HGETALL";
    args[4] = "quotekey";
    output = runCli(5, args);
    EXPECT_EQ("{\"testfield\":\"va'l\\\"ue\"}\n", output);

    // get from test db becomes a quoted JSON string
    args[3] = "GET";
    args[4] = "notexistkey";
    output = runCli(5, args);
    EXPECT_EQ("null\n", output);

    // keys from test db becomes a JSON array
    args[3] = "DEL";
    args[4] = "quotekey";
    output = runCli(5, args);
    EXPECT_EQ("1\n", output);

    args[3] = "keys";
    args[4] = "*";
    output = runCli(5, args);
    EXPECT_EQ("[\"testkey\"]\n", output);

    // long option works as well
    args[1] = "--json";
    args[3] = "HGETALL";
    args[4] = "testkey";
    output = runCli(5, args);
    EXPECT_EQ("{\"testfield\":\"testvalue\"}\n", output);
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
    char *args[3];
    args[0] = "sonic-db-cli";
    args[1] = "PING";

    auto output = runCli(2, args);
    EXPECT_EQ("PONG\n", output);

    // When ping with DB name, result should be 'True'
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";
    args[2] = "PING";

    output = runCli(3, args);
    EXPECT_EQ("True\n", output);
}

TEST(sonic_db_cli, test_cli_ping_cmd_no_config)
{
    char *args[3];
    args[0] = "sonic-db-cli";
    args[1] = "PING";

    // data base file does not exist, will throw exception
    auto initializeGlobalConfig = []()
    {
        SonicDBConfig::initializeGlobalConfig(not_exist_config_file);
    };

    auto initializeConfig = []()
    {
        SonicDBConfig::initialize(not_exist_config_file);
    };

    optind = 0;
    int exit_code = sonic_db_cli(
                    2,
                    args,
                    initializeGlobalConfig,
                    initializeConfig);

    EXPECT_EQ(1, exit_code);

    // When ping with DB name, exception will happen
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";
    args[2] = "PING";

    bool exception_happen = false;
    try
    {
        optind = 0;
        exit_code = sonic_db_cli(
                        3,
                        args,
                        initializeGlobalConfig,
                        initializeConfig);
    }
    catch (const exception& e)
    {
        exception_happen = true;
    }

    EXPECT_EQ(true, exception_happen);
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
    args[2] = "asic2";
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
    args[3] = "asic2";
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
    args[2] = "asic2";
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
    args[2] = "asic2";
    args[4] = "SAVE";

    auto db_names = swss::SonicDBConfig::getDbList("asic2");
    for (auto& db_name : db_names)
    {
        generateTestData("asic2", const_cast<char*>(db_name.c_str()));
    }
    db_names = swss::SonicDBConfig::getDbList("asic3");
    for (auto& db_name : db_names)
    {
        generateTestData("asic3", const_cast<char*>(db_name.c_str()));
    }

    // save 2 DBs and get save DB performance
    auto begin_time = clock();
    db_names = swss::SonicDBConfig::getDbList("asic2");
    args[2] = "asic2";
    for (auto& db_name : db_names)
    {
        args[3] = const_cast<char*>(db_name.c_str());
        optind = 0;
        sonic_db_cli(5, args);
    }

    db_names = swss::SonicDBConfig::getDbList("asic3");
    args[2] = "asic3";
    for (auto& db_name : db_names)
    {
        args[3] = const_cast<char*>(db_name.c_str());
        optind = 0;
        sonic_db_cli(5, args);
    }

    auto sequential_time = float( clock () - begin_time );

    // prepare data
    db_names = swss::SonicDBConfig::getDbList("asic2");
    for (auto& db_name : db_names)
    {
        generateTestData("asic2", const_cast<char*>(db_name.c_str()));
    }
    db_names = swss::SonicDBConfig::getDbList("asic3");
    for (auto& db_name : db_names)
    {
        generateTestData("asic3", const_cast<char*>(db_name.c_str()));
    }

    // save 2 DBs in parallel, and get save DB performance
    begin_time = clock();

    args[1] = "SAVE";
    optind = 0;
    sonic_db_cli(2, args);

    auto parallen_time = float( clock () - begin_time );
    EXPECT_TRUE(parallen_time < sequential_time);
}

TEST(sonic_db_cli, test_cli_not_throw_exception)
{
    char *args[5];
    args[0] = "sonic-db-cli";
    args[1] = "TEST_DB";
    
    // set key to test DB
    args[2] = "SET";
    args[3] = "testkey";
    args[4] = "testvalue";

    // data base file does not exist, will throw exception
    auto initializeGlobalConfig = []()
    {
        throw std::system_error();
    };

    auto initializeConfig = []()
    {
        throw std::system_error();
    };

    optind = 0;
    int exit_code = cli_exception_wrapper(
                        5,
                        args,
                        initializeGlobalConfig,
                        initializeConfig);

    EXPECT_EQ(1, exit_code);
}

namespace
{

class CliCapture
{
public:
    explicit CliCapture(bool standardError) : m_standardError(standardError)
    {
        if (m_standardError)
            testing::internal::CaptureStderr();
        else
            testing::internal::CaptureStdout();
    }
    ~CliCapture()
    {
        if (m_active)
            take();
    }
    string take()
    {
        m_active = false;
        return m_standardError ? testing::internal::GetCapturedStderr() :
                                 testing::internal::GetCapturedStdout();
    }
private:
    bool m_standardError;
    bool m_active = true;
};

class CliEndpointTest : public ::testing::Test
{
protected:
    void writeFile(const string& name, const string& content, mode_t mode = 0600)
    {
        ofstream output(path(name));
        output << content;
        output.close();
        ASSERT_TRUE(output.good());
        ASSERT_EQ(0, chmod(path(name).c_str(), mode));
    }

    string path(const string& name) const { return m_directory + "/" + name; }

    void SetUp() override
    {
        char pattern[] = "/tmp/swss-cli-endpoint-XXXXXX";
        char *directory = mkdtemp(pattern);
        ASSERT_NE(nullptr, directory);
        m_directory = directory;

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(fd, 0);
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_EQ(0, bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)));
        socklen_t length = sizeof(address);
        ASSERT_EQ(0, getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length));
        m_port = ntohs(address.sin_port);
        close(fd);

        writeFile("redis.conf", "bind 127.0.0.1\nprotected-mode no\nport " +
            to_string(m_port) + "\nunixsocket " + path("redis.sock") +
            "\nunixsocketperm 700\nsave \"\"\nappendonly no\ndir " + m_directory + "\nlogfile " + path("redis.log") + "\n");
        m_pid = fork();
        ASSERT_GE(m_pid, 0);
        if (m_pid == 0)
        {
            execlp("redis-server", "redis-server", path("redis.conf").c_str(),
                   static_cast<char *>(nullptr));
            _exit(127);
        }
        bool ready = false;
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            try
            {
                DBConnector probe(15, path("redis.sock"), 100);
                ready = true;
                break;
            }
            catch (const exception&)
            {
                this_thread::sleep_for(chrono::milliseconds(20));
            }
        }
        ASSERT_TRUE(ready);

        using json = nlohmann::json;
        json instances = {
            {"local", {{"hostname", "127.0.0.1"}, {"port", m_port},
                       {"unix_socket_path", path("redis.sock")}}},
            {"tcp", {{"hostname", "127.0.0.1"}, {"port", m_port}}},
            {"socket-only", {{"hostname", "127.0.0.1"}, {"port", 0},
                             {"unix_socket_path", path("redis.sock")}}},
            {"missing-socket", {{"hostname", "127.0.0.1"}, {"port", m_port},
                                {"unix_socket_path", path("missing.sock")}}},
            {"missing-selected-socket", {{"hostname", "127.0.0.1"}, {"port", 0},
                                         {"unix_socket_path", path("missing.sock")}}},
            {"chassis", {{"hostname", "redis_chassis.server"}, {"port", m_port},
                         {"unix_socket_path", path("redis.sock")}}}
        };
        json databases;
        for (const auto& entry : vector<pair<string, string>>{
                 {"LOCAL_DB", "local"}, {"TCP_DB", "tcp"},
                 {"SOCKET_ONLY_DB", "socket-only"},
                 {"MISSING_SOCKET_DB", "missing-socket"},
                 {"MISSING_SELECTED_SOCKET_DB", "missing-selected-socket"},
                 {"CHASSIS_DB", "chassis"}})
        {
            databases[entry.first] = {{"id", 15}, {"separator", ":"},
                                      {"instance", entry.second}};
        }
        writeFile("database.json", json({{"INSTANCES", instances},
                                         {"DATABASES", databases}, {"VERSION", "1.0"}}).dump());
        writeFile("global.json", json({{"INCLUDES", json::array({
            {{"include", "database.json"}},
            {{"namespace", "asic9"}, {"include", "database.json"}}})},
            {"VERSION", "1.0"}}).dump());
        SonicDBConfig::reset();
        SonicDBConfig::initialize(path("database.json"));
        SonicDBConfig::initializeGlobalConfig(path("global.json"));
    }

    void startNamespaceServer()
    {
        writeFile("asic-redis.conf", "bind 127.0.0.1\nprotected-mode no\nport 0\nunixsocket " +
            path("asic.sock") + "\nunixsocketperm 700\nsave \"\"\nappendonly no\ndir " + m_directory + "\nlogfile " + path("asic-redis.log") + "\n");
        m_namespacePid = fork();
        ASSERT_GE(m_namespacePid, 0);
        if (m_namespacePid == 0)
        {
            execlp("redis-server", "redis-server", path("asic-redis.conf").c_str(),
                   static_cast<char *>(nullptr));
            _exit(127);
        }
        bool ready = false;
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            try
            {
                DBConnector probe(15, path("asic.sock"), 100);
                ready = true;
                break;
            }
            catch (const exception&)
            {
                this_thread::sleep_for(chrono::milliseconds(20));
            }
        }
        ASSERT_TRUE(ready);

        using json = nlohmann::json;
        auto namespaceConfig = json::parse(readFileContent(path("database.json")));
        // Match real ASIC metadata: a distinct socket, but the host's TCP address and port.
        namespaceConfig["INSTANCES"]["local"]["unix_socket_path"] = path("asic.sock");
        writeFile("namespace.json", namespaceConfig.dump());
        writeFile("global.json", json({{"INCLUDES", json::array({
            {{"include", "database.json"}},
            {{"namespace", "asic9"}, {"include", "namespace.json"}}})},
            {"VERSION", "1.0"}}).dump());
        SonicDBConfig::reset();
        SonicDBConfig::initialize(path("database.json"));
        SonicDBConfig::initializeGlobalConfig(path("global.json"));
    }

    void TearDown() override
    {
        if (m_namespacePid > 0)
        {
            kill(m_namespacePid, SIGTERM);
            waitpid(m_namespacePid, nullptr, 0);
        }
        if (m_pid > 0)
        {
            kill(m_pid, SIGTERM);
            waitpid(m_pid, nullptr, 0);
        }
        for (const char *name : {"redis.conf", "redis.log", "redis.sock", "database.json", "global.json",
                "asic-redis.conf", "asic-redis.log", "asic.sock", "namespace.json"})
        {
            unlink(path(name).c_str());
        }
        if (!m_directory.empty())
        {
            rmdir(m_directory.c_str());
        }
        SonicDBConfig::reset();
        SonicDBConfig::initialize(config_file);
        SonicDBConfig::initializeGlobalConfig(global_config_file);
    }

    string run(vector<string> arguments, int expected = 0)
    {
        vector<char *> argv;
        for (auto& argument : arguments)
        {
            argv.push_back(const_cast<char *>(argument.c_str()));
        }
        try
        {
            return runCli(static_cast<int>(argv.size()), argv.data(), expected);
        }
        catch (...)
        {
            // runCli starts its own stdout capture before invoking the CLI.
            testing::internal::GetCapturedStdout();
            throw;
        }
    }

    string m_directory;
    int m_port = 0;
    pid_t m_pid = -1;
    pid_t m_namespacePid = -1;
};

TEST_F(CliEndpointTest, DefaultSelectsSocketForTcpDisabledProcess)
{
    auto db = connectToDatabase("SOCKET_ONLY_DB", "", CliEndpointType::AUTO);
    EXPECT_EQ(REDIS_CONN_UNIX, db->getContext()->connection_type);
    EXPECT_EQ("True\n", run({"sonic-db-cli", "SOCKET_ONLY_DB", "PING"}));
}

TEST_F(CliEndpointTest, DefaultPreservesTcpForLegacyProcessWithConfiguredSocket)
{
    auto db = connectToDatabase("LOCAL_DB", "", CliEndpointType::AUTO);
    EXPECT_EQ(REDIS_CONN_TCP, db->getContext()->connection_type);
    EXPECT_EQ("True\n", run({"sonic-db-cli", "LOCAL_DB", "PING"}));
    auto explicitSocket = connectToDatabase("LOCAL_DB", "", CliEndpointType::UNIX_SOCKET);
    EXPECT_EQ(REDIS_CONN_UNIX, explicitSocket->getContext()->connection_type);
    // A legacy process must stay usable even when its configured socket cannot be opened.
    EXPECT_EQ("True\n", run({"sonic-db-cli", "MISSING_SOCKET_DB", "PING"}));
    EXPECT_THROW(connectToDatabase("MISSING_SOCKET_DB", "", CliEndpointType::UNIX_SOCKET), exception);
}

TEST_F(CliEndpointTest, DefaultSelectsTcpWhenMetadataHasNoSocket)
{
    auto db = connectToDatabase("TCP_DB", "", CliEndpointType::AUTO);
    EXPECT_EQ(REDIS_CONN_TCP, db->getContext()->connection_type);
    EXPECT_EQ("True\n", run({"sonic-db-cli", "TCP_DB", "PING"}));
}

TEST_F(CliEndpointTest, ExplicitTcpOverridesSocketAndNamespaceDefault)
{
    auto db = connectToDatabase("LOCAL_DB", "", CliEndpointType::TCP);
    EXPECT_EQ(REDIS_CONN_TCP, db->getContext()->connection_type);
    EXPECT_EQ("True\n", run({"sonic-db-cli", "--tcp", "-n", "asic9",
                                  "MISSING_SOCKET_DB", "PING"}));
}

TEST_F(CliEndpointTest, NamespaceAutoReadsAndWritesOnlyItsSocketBackend)
{
    startNamespaceServer();
    ASSERT_FALSE(HasFatalFailure());
    auto host = connectToDatabase("LOCAL_DB", "", CliEndpointType::UNIX_SOCKET);
    auto asic = connectToDatabase("LOCAL_DB", "asic9", CliEndpointType::AUTO);
    EXPECT_EQ(REDIS_CONN_UNIX, asic->getContext()->connection_type);
    ASSERT_TRUE(host->set("namespace-sentinel", "host"));
    ASSERT_TRUE(asic->set("namespace-sentinel", "asic"));
    EXPECT_EQ("asic\n", run({"sonic-db-cli", "-n", "asic9", "LOCAL_DB",
                                "GET", "namespace-sentinel"}));
    vector<string> command = {"SET", "namespace-sentinel", "updated-asic"};
    CliCapture output(false);
    EXPECT_EQ(0, executeCommands("LOCAL_DB", command, "asic9", CliEndpointType::AUTO));
    EXPECT_EQ("True\n", output.take());
    auto hostValue = host->get("namespace-sentinel");
    auto asicValue = asic->get("namespace-sentinel");
    ASSERT_NE(nullptr, hostValue);
    ASSERT_NE(nullptr, asicValue);
    EXPECT_EQ("host", *hostValue);
    EXPECT_EQ("updated-asic", *asicValue);
    EXPECT_EQ("host\n", run({"sonic-db-cli", "-t", "-n", "asic9", "LOCAL_DB",
                                "GET", "namespace-sentinel"}));
}

TEST_F(CliEndpointTest, NamespaceAutoRequiresSocketAndNeverFallsBackToHostTcp)
{
    EXPECT_THROW(connectToDatabase("TCP_DB", "asic9", CliEndpointType::AUTO), invalid_argument);
    EXPECT_THROW(connectToDatabase("MISSING_SOCKET_DB", "asic9", CliEndpointType::AUTO), exception);
    CliCapture errors(true);
    EXPECT_EQ("", run({"sonic-db-cli", "-n", "asic9", "TCP_DB", "PING"}, 1));
    EXPECT_NE(string::npos, errors.take().find("No Unix socket is configured"));
    EXPECT_EQ("True\n", run({"sonic-db-cli", "-t", "-n", "asic9", "TCP_DB", "PING"}));
}

TEST_F(CliEndpointTest, ExplicitTcpDoesNotFallBackToWorkingSocket)
{
    EXPECT_THROW(connectToDatabase("SOCKET_ONLY_DB", "", CliEndpointType::TCP), exception);
    CliCapture errors(true);
    EXPECT_EQ("", run({"sonic-db-cli", "-t", "SOCKET_ONLY_DB", "PING"}, 1));
    errors.take();
}

TEST_F(CliEndpointTest, SelectedSocketFailureDoesNotChooseAnotherEndpoint)
{
    EXPECT_THROW(connectToDatabase("MISSING_SELECTED_SOCKET_DB", "", CliEndpointType::AUTO), exception);
    CliCapture errors(true);
    EXPECT_EQ("", run({"sonic-db-cli", "MISSING_SELECTED_SOCKET_DB", "PING"}, 1));
    errors.take();
}

TEST_F(CliEndpointTest, ExplicitSocketRequiresConfiguredSocket)
{
    EXPECT_THROW(connectToDatabase("TCP_DB", "", CliEndpointType::UNIX_SOCKET), invalid_argument);
}

TEST_F(CliEndpointTest, ChassisKeepsTcpEvenWhenSocketIsAvailable)
{
    for (auto mode : {CliEndpointType::AUTO, CliEndpointType::UNIX_SOCKET})
    {
        try
        {
            auto db = connectToDatabase("CHASSIS_DB", "", mode);
            EXPECT_EQ(REDIS_CONN_TCP, db->getContext()->connection_type);
        }
        catch (const system_error& e)
        {
            // The test host need not resolve the real chassis hostname.
            // A failed TCP connection must not fall back to the working socket.
            EXPECT_EQ(make_error_code(errc::address_not_available), e.code());
        }
    }
}

TEST_F(CliEndpointTest, TransportFlagsAreMutuallyExclusiveInEitherOrder)
{
    for (auto arguments : {vector<string>{"sonic-db-cli", "-s", "-t", "LOCAL_DB", "PING"},
                           vector<string>{"sonic-db-cli", "--tcp", "--unixsocket", "LOCAL_DB", "PING"}})
    {
        CliCapture errors(true);
        auto output = run(arguments, -1);
        EXPECT_NE(string::npos, output.find("usage: sonic-db-cli"));
        EXPECT_NE(string::npos, errors.take().find("mutually exclusive"));
    }
}

} // namespace
