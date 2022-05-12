#include "sonic-db-cli.h"
#include "common/dbconnector.h"

int main(int argc, char** argv)
{
    return sonic_db_cli(
                    swss::SonicDBConfig::DEFAULT_SONIC_DB_CONFIG_FILE,
                    swss::SonicDBConfig::DEFAULT_SONIC_DB_GLOBAL_CONFIG_FILE,
                    argc,
                    argv);
}