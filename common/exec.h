#pragma once

#include <string>

namespace swss {

enum EXEC_RC
{
    EXEC_ERROR_PCLOSE = -1,
    EXEC_ERROR_SIGNALED = -2,
    EXEC_ERROR_STOPPED = -3,
    EXEC_ERROR_CONTINUED = -4,
    EXEC_ERROR_UNEXPECTED = -16,
};

int exec(const std::string &cmd, std::string &stdout);

}
