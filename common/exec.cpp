#include <array>
#include "exec.h"
#include "common/logger.h"

using namespace std;

namespace swss {

string exec(const char* cmd)
{
    array<char, 128> buffer;
    string result;
    shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);

    if (!pipe)
    {
        string errmsg(cmd);

        errmsg = "popen(" + errmsg + ") failed!";
        SWSS_LOG_ERROR("exec: %s", errmsg.c_str());
        throw runtime_error(errmsg);
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
