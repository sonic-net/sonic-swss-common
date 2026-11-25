#include "sonic-db-cli.h"

int main()
{
    char program[] = "sonic-db-cli";
    char help[] = "--help";
    char *arguments[] = {program, help, nullptr};
    bool initialized = false;
    auto initialize = [&initialized]() { initialized = true; };
    const int result = cli_exception_wrapper(2, arguments, initialize, initialize);
    return result == 0 && !initialized ? 0 : 1;
}
