#include "sonic-db-cli.h"
#include "common/dbconnector.h"
#include <iostream>

using namespace swss;
using namespace std;

int main(int argc, char** argv)
{
    auto initializeGlobalConfig = []()
    {
        SonicDBConfig::initializeGlobalConfig(SonicDBConfig::DEFAULT_SONIC_DB_GLOBAL_CONFIG_FILE);
    };

    auto initializeConfig = []()
    {
        SonicDBConfig::initialize(SonicDBConfig::DEFAULT_SONIC_DB_CONFIG_FILE);
    };

    return cli_exception_wrapper(
                    argc,
                    argv,
                    initializeGlobalConfig,
                    initializeConfig);
}