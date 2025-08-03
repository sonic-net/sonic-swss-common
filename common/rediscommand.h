#pragma once
#include <cstring>
#include <utility>
#include <tuple>
#include <vector>
#include <string>
#include <stdexcept>
#include <map>
#include <hiredis/hiredis.h>

namespace swss {

typedef std::pair<std::string, std::string> FieldValueTuple;
#define fvField std::get<0>
#define fvValue std::get<1>
typedef std::tuple<std::string, std::string, std::vector<FieldValueTuple> > KeyOpFieldsValuesTuple;
#define kfvKey    std::get<0>
#define kfvOp     std::get<1>
#define kfvFieldsValues std::get<2>


class RedisCommand {
public:
    RedisCommand();
    ~RedisCommand();
    RedisCommand(RedisCommand& that) = delete;
    RedisCommand& operator=(RedisCommand& that) = delete;

    void format(const char *fmt, ...);
    void formatArgv(int argc, const char **argv, const size_t *argvlen);
    void format(const std::vector<std::string> &commands);

    /* Format HMSET key multiple field value command */
#ifndef SWIG
    __attribute__((deprecated))
#endif
    void formatHSET(const std::string &key,
                     const std::vector<FieldValueTuple> &values);

    void formatHSET(const std::string &key,
                     const std::map<std::string, std::string> &values);

    template<typename InputIterator>
    void formatHSET(const std::string &key,
                     InputIterator start, InputIterator stop);

    /* Format HSET key field value command */
    void formatHSET(const std::string& key, const std::string& field,
                    const std::string& value);

    /* Format HGET key field command */
    void formatHGET(const std::string& key, const std::string& field);

    /* Format HDEL key field command */
    void formatHDEL(const std::string& key, const std::string& field);

    /* Format HDEL key multiple fields command */
    void formatHDEL(const std::string& key, const std::vector<std::string>& fields);

    /* Format EXPIRE key ttl command */
    void formatEXPIRE(const std::string& key, const int64_t& ttl);

    /* Format TTL key command */
    void formatTTL(const std::string& key);

    /* Format DEL key command */
    void formatDEL(const std::string& key);

    int appendTo(redisContext *ctx) const;

    std::string toPrintableString() const;

protected:
    const char *c_str() const;

    size_t length() const;

private:
    char *temp;
    long long len;
};

template<typename InputIterator>
void RedisCommand::formatHSET(const std::string &key,
                 InputIterator start, InputIterator stop)
{
    if (start == stop) throw std::invalid_argument("empty values");

    const char* cmd = "HSET";

    std::vector<std::string> args = { cmd, key.c_str() };

    for (auto i = start; i != stop; i++)
    {
        args.push_back(fvField(*i));
        args.push_back(fvValue(*i));
    }

    format(args);
}

}
