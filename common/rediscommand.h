#pragma once
#include <string.h>
#include <utility>
#include <tuple>

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

    void format(const char *fmt, ...);
    void formatArgv(int argc, const char **argv, const size_t *argvlen);

    /* Format HMSET key multiple field value command */
    void formatHMSET(const std::string &key,
                     const std::vector<FieldValueTuple> &values);

    /* Format HSET key field value command */
    void formatHSET(const std::string& key, const std::string& field,
                    const std::string& value);

    /* Format HGET key field command */
    void formatHGET(const std::string& key, const std::string& field);

    /* Format HDEL key field command */
    void formatHDEL(const std::string& key, const std::string& field);

    const char *c_str() const;

    size_t length() const;

private:
    char *temp;
};

}
