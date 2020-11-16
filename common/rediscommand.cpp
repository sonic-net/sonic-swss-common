#include <vector>
#include <hiredis/hiredis.h>
#include "rediscommand.h"

using namespace std;

namespace swss {

RedisCommand::RedisCommand()
 : temp(NULL)
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

    va_list ap;
    va_start(ap, fmt);
    int len = redisvFormatCommand(&temp, fmt, ap);
    va_end(ap);
    if (len == -1) {
        throw std::bad_alloc();
    } else if (len == -2) {
        throw std::invalid_argument("fmt");
    }
}

void RedisCommand::formatArgv(int argc, const char **argv, const size_t *argvlen)
{
    if (temp != nullptr)
    {
        redisFreeCommand(temp);
        temp = nullptr;
    }

    int len = redisFormatCommandArgv(&temp, argc, argv, argvlen);
    if (len == -1) {
        throw std::bad_alloc();
    }
}

/* Format HMSET key multiple field value command */
void RedisCommand::formatHMSET(const std::string &key,
                               const std::vector<FieldValueTuple> &values)
{
    formatHMSET(key, values.begin(), values.end());
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

    std::vector<const char *> args = {"HDEL", key.c_str()};
    for (const std::string &f : fields)
    {
        args.push_back(f.c_str());
    }
    formatArgv(static_cast<int>(args.size()), args.data(), NULL);
}

const char *RedisCommand::c_str() const
{
    return temp;
}

size_t RedisCommand::length() const
{
    return strlen(temp);
}

}
