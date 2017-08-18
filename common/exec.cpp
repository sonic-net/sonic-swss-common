#include "exec.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <array>
#include "common/logger.h"

using namespace std;

namespace swss {

std::string exec(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);

    if (!pipe)
    {
        std::string errmsg(cmd);

        errmsg = "popen(" + errmsg + ") failed!";
        SWSS_LOG_ERROR("exec: %s", errmsg.c_str());
        throw std::runtime_error(errmsg);
    }

    while (!feof(pipe.get()))
    {
        if (fgets(buffer.data(), 128, pipe.get()) != NULL)
            result += buffer.data();
    }

    SWSS_LOG_DEBUG("%s : %s", cmd, result.c_str());
    return result;
}

}