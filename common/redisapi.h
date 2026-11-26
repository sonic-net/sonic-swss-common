#pragma once
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <sys/stat.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <fstream>
#include <algorithm>
#include "logger.h"
#include "rediscommand.h"
#include "redisreply.h"
#include "dbconnector.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

namespace swss {

static inline std::string loadRedisScript(RedisContext* ctx, const std::string& script)
{
    SWSS_LOG_ENTER();

    RedisCommand loadcmd;
    loadcmd.format("SCRIPT LOAD %s", script.c_str());
    RedisReply r(ctx, loadcmd, REDIS_REPLY_STRING);

    std::string sha = r.getReply<std::string>();

    SWSS_LOG_INFO("lua script %s loaded, sha: %s", script.c_str(), sha.c_str());

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

    // Ignore process-environment overrides across a privileged exec boundary.
    const char *directory = secure_getenv("SWSS_COMMON_LUA_DIR");
    if (!directory || !*directory)
    {
        return readTextFile("/usr/share/swss/" + path);
    }
    if (path.empty() || path == "." || path == ".." ||
        path.find_first_of("/\\") != std::string::npos)
    {
        throw std::invalid_argument("Lua script must be a single file name");
    }
    if (directory[0] != '/')
    {
        throw std::invalid_argument("SWSS_COMMON_LUA_DIR must be absolute");
    }

    // The operator selects the runtime root. Resolve it once and confine the
    // requested script to that root, including the target of any symlink.
    std::unique_ptr<char, decltype(&std::free)> resolvedDirectory(
        realpath(directory, nullptr), std::free);
    if (!resolvedDirectory)
    {
        SWSS_LOG_THROW("failed to resolve Lua directory '%s': %s", directory, strerror(errno));
    }
    struct stat metadata;
    if (stat(resolvedDirectory.get(), &metadata) != 0 || !S_ISDIR(metadata.st_mode))
    {
        throw std::invalid_argument("SWSS_COMMON_LUA_DIR must be a directory");
    }
    std::string root = resolvedDirectory.get();
    if (root != "/") root += "/";
    const std::string requested = root + path;
    std::unique_ptr<char, decltype(&std::free)> resolvedScript(
        realpath(requested.c_str(), nullptr), std::free);
    if (!resolvedScript)
    {
        SWSS_LOG_THROW("failed to read file: '%s': %s", requested.c_str(), strerror(errno));
    }
    const std::string script = resolvedScript.get();
    if (script.compare(0, root.size(), root) != 0)
    {
        throw std::invalid_argument("Lua script escapes its configured directory");
    }
    return readTextFile(script);
}

static inline std::set<std::string> runRedisScript(RedisContext &ctx, const std::string& sha,
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

    RedisCommand command;
    command.format(args);

    std::set<std::string> ret;
    try
    {
        RedisReply r(&ctx, command);
        auto reply = r.getContext();
        SWSS_LOG_DEBUG("Running lua script %s", sha.c_str());

        if (reply->type == REDIS_REPLY_NIL)
        {
            SWSS_LOG_ERROR("Got EMPTY response type from redis %d", reply->type);
        }
        else if (reply->type == REDIS_REPLY_INTEGER)
        {
            SWSS_LOG_DEBUG("Got INTEGER response type from redis %d", reply->type);
            ret.emplace(std::to_string(reply->integer));
        }
        else if (reply->type != REDIS_REPLY_ARRAY)
        {
            SWSS_LOG_ERROR("Got invalid response type from redis %d", reply->type);
        }
        else
        {
            for (size_t i = 0; i < reply->elements; i++)
            {
                SWSS_LOG_DEBUG("Got element %zu %s", i, reply->element[i]->str);
                ret.emplace(reply->element[i]->str);
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

static inline void lazyLoadRedisScriptFile(RedisContext* ctx, std::string luaPath, std::string &sha)
{
    if (sha.empty())
    {
        std::string luaScript = loadLuaScript(luaPath);
        sha = loadRedisScript(ctx, luaScript);
    }
}

}
