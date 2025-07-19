#include <cerrno>
#include <cstring>
#include <array>
#include <sys/wait.h>
#include <stdexcept>
#include "exec.h"
#include "common/logger.h"

using namespace std;

namespace swss {

int exec(const string &cmd, string &stdout)
{
    const int buffsz = 128;
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

    int ret;
    string msg;
    int status = pclose(pipe);
    if (status == -1)
    {
        ret = EXEC_ERROR_PCLOSE;
        SWSS_LOG_ERROR("%s: %s", cmd.c_str(), strerror(errno));
    }
    else if (WIFEXITED(status))
    {
        ret = WEXITSTATUS(status);
        msg = "Exited with rc=" + to_string(ret);
    }
    else if (WIFSIGNALED(status))
    {
        ret = EXEC_ERROR_SIGNALED;
        msg = "Killed with signal=" + to_string(WTERMSIG(status));
    }
    else if (WIFSTOPPED(status))
    {
        ret = EXEC_ERROR_STOPPED;
        msg = "Stopped with signal=" + to_string(WSTOPSIG(status));
    }
#ifdef WIFCONTINUED     /* Not all implementations support this */
    else if (WIFCONTINUED(status))
    {
        ret = EXEC_ERROR_CONTINUED;
        msg = "Continued";
    }
#endif
    else
    {
        ret = EXEC_ERROR_UNEXPECTED;
        msg = "Unexpected pclose ret=" + to_string(status);
    }

    SWSS_LOG_DEBUG("%s : %s : %s", cmd.c_str(), stdout.c_str(), msg.c_str());

    return ret;
}

}
