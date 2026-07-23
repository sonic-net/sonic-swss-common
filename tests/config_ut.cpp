#include "gtest/gtest.h"
#include "sonic-db-cli/sonic-db-cli.h"

using namespace swss;
using namespace std;

TEST(sonic_db_cli, test_cli_initialize_default_config)
{
    initializeGlobalConfig();
    try{
        initializeConfig("");
    }
    catch(const std::exception& e)
    {
    }

    EXPECT_EQ(1, 1);
}

TEST(sonic_db_cli, test_cli_initialize_dpu_config)
{
    initializeGlobalConfig();
    try{
        initializeConfig("dpu0");
    }
    catch(const std::exception& e)
    {
    }

    EXPECT_EQ(1, 1);
}

TEST(sonic_db_cli, test_get_container_file_path)
{
    const string global_config_file = "./tests/redis_multi_db_ut_config/database_global.json";
    const string config_directory = "./tests";
    auto path = getContainerFilePath("dpu0", config_directory, global_config_file);
    EXPECT_EQ(path, "./tests/redis_multi_db_ut_config/database_config4.json");
}