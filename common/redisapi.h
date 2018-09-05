#pragma once
#include <unistd.h>
#include <poll.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <fstream>
#include <algorithm>
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

    return readTextFile("/usr/share/swss/" + path);
}

static inline std::set<std::string> runRedisScript(DBConnector &db, const std::string& sha,
        const std::vector<std::string>& keys, const std::vector<std::string>& argv)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> args;

    // Prepare EVALSHA command
    // Format is following:
    // EVALSHA <sha> <size of KEYS> <KEYS> <ARGV>
    args.push_back("EVALSHA");
    args.push_back(sha);
    args.push_back(std::to_string(keys.size()));
    args.insert(args.end(), keys.begin(), keys.end());
    args.insert(args.end(), argv.begin(), argv.end());
    args.push_back("''");

    // Convert to vector of char *
    std::vector<const char *> c_args;
    transform(
            args.begin(),
            args.end(),
            std::back_inserter(c_args),
            [](const std::string& s) { return s.c_str(); } );

    RedisCommand command;
    command.formatArgv(static_cast<int>(c_args.size()), c_args.data(), NULL);

    std::set<std::string> ret;
    try
    {
        RedisReply r(&db, command);
        auto ctx = r.getContext();
        SWSS_LOG_DEBUG("Running lua script %s", sha.c_str());

        if (ctx->type == REDIS_REPLY_NIL)
        {
            SWSS_LOG_ERROR("Got EMPTY response type from redis %d", ctx->type);
        }
        else if (ctx->type != REDIS_REPLY_ARRAY)
        {
            SWSS_LOG_ERROR("Got invalid response type from redis %d", ctx->type);
        }
        else
        {
            for (size_t i = 0; i < ctx->elements; i++)
            {
                SWSS_LOG_DEBUG("Got element %lu %s", i, ctx->element[i]->str);
                ret.emplace(ctx->element[i]->str);
            }
        }
    }
    catch (const std::exception& e)
    {
        SWSS_LOG_ERROR("Caught exception while running Redis lua script: %s", e.what());
    }
    catch(...)
    {
        SWSS_LOG_ERROR("Caught exception while running Redis lua script");
    }

    return ret;
}

static inline int peekRedisContext(redisContext *c)
{
    pollfd fd;
    fd.fd = c->fd;
    fd.events = POLLIN;

    int rc = poll(&fd, 1, 0);
    return rc;
}

}
