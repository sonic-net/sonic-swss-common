#include <cstdlib>
#include <iostream>
#include "loglevel.h"
#include "redisauth.h"

int main(int argc, char** argv)
{
    try
    {
        return swssloglevel(argc, argv);
    }
    catch (const swss::RedisAuthenticationError& error)
    {
        std::cerr << "swssloglevel: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}
