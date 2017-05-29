#pragma once
#include <unistd.h>
#include <stdexcept>
#include <vector>
#include <fstream>
#include "logger.h"
#include "rediscommand.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

namespace swss {

static inline std::string loadRedisScript(DBConnector* db, const std::string& script)
{
    SWSS_LOG_ENTER();

    RedisCommand loadcmd;
    loadcmd.format("SCRIPT LOAD %s", script.c_str());
    RedisReply r(db, loadcmd, REDIS_REPLY_STRING);

    std::string sha = r.getReply<std::string>();

    SWSS_LOG_NOTICE("lua script loaded, sha: %s", sha.c_str());
    SWSS_LOG_INFO("lua script: %s", script.c_str());

    return sha;
}

// Hit Redis bug: Lua redis() command arguments must be strings or integers,
//   however, an empty string is not accepted
// Bypass by prefix a dummy char and remove it in the lua script
static inline std::string encodeLuaArgument(const std::string& arg)
{
    return '`' + arg;
}

inline bool fileExists(const std::string& name)
{
    return access(name.c_str(), F_OK) != -1;
}

inline std::string readTextFile(const std::string& path)
{
    std::ifstream ifs(path);

    if (ifs.good())
    {
        return std::string((std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));
    }

    SWSS_LOG_THROW("failed to read file: '%s': %s", path.c_str(), strerror(errno));
}

static inline std::string loadLuaScript(const std::string& path)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> paths = {
        path,
        CONFIGURE_PREFIX "/" + path,
        "/usr/share/swss/" + path,
        "/usr/local/share/swss/" + path,
    };

    for (auto p: paths)
    {
        if (fileExists(p))
        {
            return readTextFile(p);
        }
    }

    SWSS_LOG_THROW("failed to locate file: %s", path.c_str());
}

}
