#include <cerrno>
#include <cstring>
#include <array>
#include "exec.h"
#include "common/logger.h"

using namespace std;

namespace swss {

const int buffsz = 128;

int exec(const string &cmd, string &stdout)
{
    array<char, buffsz> buffer;
    FILE* pipe = popen(cmd.c_str(), "r");

    if (!pipe)
    {
        string errmsg(cmd);

        errmsg = "popen(" + errmsg + ") failed!";
        SWSS_LOG_ERROR("exec: %s", errmsg.c_str());
        throw runtime_error(errmsg);
    }

    stdout.clear();
    while (!feof(pipe))
    {
        if (fgets(buffer.data(), buffsz, pipe) != NULL)
        {
            stdout += buffer.data();
        }
    }

    int ret = pclose(pipe);
    if (ret != 0)
    {
        SWSS_LOG_ERROR("%s: %s", cmd.c_str(), strerror(errno));
    }
    SWSS_LOG_DEBUG("%s : %s", cmd.c_str(), stdout.c_str());

    return ret;
}

}
