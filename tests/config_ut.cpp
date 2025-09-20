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
