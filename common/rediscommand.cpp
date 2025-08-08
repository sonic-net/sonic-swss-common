#include <vector>
#include <hiredis/hiredis.h>
#include "rediscommand.h"
#include "stringutility.h"

using namespace std;

namespace swss {

RedisCommand::RedisCommand()
 : temp(NULL),
   len(0)
{
}

RedisCommand::~RedisCommand()
{
    redisFreeCommand(temp);
}

void RedisCommand::format(const char *fmt, ...)
{
    if (temp != nullptr)
    {
        redisFreeCommand(temp);
        temp = nullptr;
    }
    len = 0;

    va_list ap;
    va_start(ap, fmt);
    int ret = redisvFormatCommand(&temp, fmt, ap);
    va_end(ap);
    if (ret == -1) {
        throw std::bad_alloc();
    } else if (ret == -2) {
        throw std::invalid_argument("fmt");
    }
    len = ret;
}

void RedisCommand::formatArgv(int argc, const char **argv, const size_t *argvlen)
{
    if (temp != nullptr)
    {
        redisFreeCommand(temp);
        temp = nullptr;
    }
    len = 0;

    long long ret = redisFormatCommandArgv(&temp, argc, argv, argvlen);
    if (ret == -1) {
        throw std::bad_alloc();
    }
    len = (int)ret;
}

void RedisCommand::format(const vector<string> &commands)
{
    vector<const char*> args;
    vector<size_t> lens;
    for (auto& command : commands)
    {
        args.push_back(command.c_str());
        lens.push_back(command.size());
    }
    formatArgv(static_cast<int>(args.size()), args.data(), lens.data());
}

/* Format HSET key multiple field value command */
void RedisCommand::formatHSET(const std::string &key,
                               const std::vector<FieldValueTuple> &values)
{
    formatHSET(key, values.begin(), values.end());
}

void RedisCommand::formatHSET(const std::string &key,
                              const std::map<std::string, std::string> &values)
{
    formatHSET(key, values.begin(), values.end());
}

/* Format HSET key field value command */
void RedisCommand::formatHSET(const std::string& key, const std::string& field,
                              const std::string& value)
{
    format("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
}

/* Format HGET key field command */
void RedisCommand::formatHGET(const std::string& key, const std::string& field)
{
    format("HGET %s %s", key.c_str(), field.c_str());
}

/* Format HDEL key field command */
void RedisCommand::formatHDEL(const std::string& key, const std::string& field)
{
    return format("HDEL %s %s", key.c_str(), field.c_str());
}

/* Format HDEL key multiple fields command */
void RedisCommand::formatHDEL(const std::string& key, const std::vector<std::string>& fields)
{
    if (fields.empty()) throw std::invalid_argument("empty values");

    std::vector<string> args = {"HDEL", key};
    args.insert(args.end(), fields.begin(), fields.end());
    format(args);
}

/* Format EXPIRE key field command */
void RedisCommand::formatEXPIRE(const std::string& key, const int64_t& ttl)
{
    return format("EXPIRE %s %lld", key.c_str(), ttl);
}

/* Format TTL key command */
void RedisCommand::formatTTL(const std::string& key)
{
    return format("TTL %s", key.c_str());
}

/* Format DEL key command */
void RedisCommand::formatDEL(const std::string& key)
{
    return format("DEL %s", key.c_str());
}

int RedisCommand::appendTo(redisContext *ctx) const
{
    return redisAppendFormattedCommand(ctx, c_str(), length());
}

std::string RedisCommand::toPrintableString() const
{
    return binary_to_printable(temp, len);
}

const char *RedisCommand::c_str() const
{
    if (len == 0)
        return nullptr;
    return temp;
}

size_t RedisCommand::length() const
{
    if (len <= 0)
        return 0;
    return static_cast<size_t>(len);
}

}
