#include "sonic-db-cli.h"
#include "common/dbconnector.h"
#include <iostream>

using namespace swss;
using namespace std;

int main(int argc, char** argv)
{
    return cli_exception_wrapper(
                    argc,
                    argv,
                    initializeGlobalConfig,
                    initializeConfig);
}