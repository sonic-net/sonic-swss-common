#pragma once
#include <stdexcept>
#include "logger.h"
#include "rediscommand.h"

namespace swss {

static inline std::string loadRedisScript(DBConnector* db, const std::string& script)
{
    SWSS_LOG_ENTER();

    RedisCommand loadcmd;
    loadcmd.format("SCRIPT LOAD %s", script.c_str());
    RedisReply r(db, loadcmd, REDIS_REPLY_STRING);

    std::string sha = r.getReply<std::string>();

    SWSS_LOG_NOTICE("lua script loaded, sha: %s", sha.c_str(), script.c_str());
    SWSS_LOG_INFO("lua script content, sha: %s", script.c_str());

    return sha;
}

// Hit Redis bug: Lua redis() command arguments must be strings or integers,
//   however, an empty string is not accepted
// Bypass by prefix a dummy char and remove it in the lua script
static inline std::string encodeLuaArgument(const std::string& arg)
{
    return '`' + arg;
}

}
