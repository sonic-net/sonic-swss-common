#include "sonic-db-cli.h"
#include "common/dbconnector.h"

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


    try
    {
        return sonic_db_cli(
                        argc,
                        argv,
                        initializeGlobalConfig,
                        initializeConfig);
    }
    catch (const exception& e)
    {
        // sonic-db-cli is porting from python version.
        // in python version, when any exception happen sonic-db-cli will crash but will not generate core dump file.
        // catch all exception here to avoid a core dump file generated.
        cerr << "An exception of type " << e.what() << " occurred. Arguments:" << endl;
        for (int idx = 0; idx < argc; idx++)
        {
            cerr << argv[idx]  << " ";
        }
        cerr << endl;

        // when python version crash, exit code is 1.
        return 1;
    }
}