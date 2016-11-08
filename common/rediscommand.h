#pragma once
#include <string.h>
#include <tuple>

namespace swss {

typedef std::tuple<std::string, std::string> FieldValueTuple;
#define fvField std::get<0>
#define fvValue std::get<1>
typedef std::tuple<std::string, std::string, std::vector<FieldValueTuple> > KeyOpFieldsValuesTuple;
#define kfvKey    std::get<0>
#define kfvOp     std::get<1>
#define kfvFieldsValues std::get<2>

class RedisCommand {
public:
    RedisCommand() : temp(NULL) { }
    ~RedisCommand() { redisFreeCommand(temp); }
    RedisCommand(RedisCommand& that);
    RedisCommand& operator=(RedisCommand& that);
    
    void format(const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int len = redisvFormatCommand(&temp, fmt, ap);
        if (len == -1) {
            throw std::bad_alloc();
        } else if (len == -2) {
            throw std::invalid_argument("fmt");
        }
    }
    
    void formatArgv(int argc, const char **argv, const size_t *argvlen) {
        int len = redisFormatCommandArgv(&temp, argc, argv, argvlen);
        if (len == -1) {
            throw std::bad_alloc();
        }
    }
    
    /* Format HMSET key multiple field value command */
    void formatHMSET(const std::string &key,
                     const std::vector<FieldValueTuple> &values)
    {
        if (values.empty()) throw std::invalid_argument("empty values");

        const char* cmd = "HMSET";

        std::vector<const char*> args = { cmd, key.c_str() };

        for (const auto &fvt: values)
        {
            args.push_back(fvField(fvt).c_str());
            args.push_back(fvValue(fvt).c_str());
        }
        
        formatArgv((int)args.size(), args.data(), NULL);
    }

    /* Format HSET key field value command */
    void formatHSET(const std::string& key, const std::string& field,
                    const std::string& value)
    {
        format("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    }

    /* Format HGET key field command */
    void formatHGET(const std::string& key, const std::string& field)
    {
        format("HGET %s %s", key.c_str(), field.c_str());
    }

    /* Format HDEL key field command */
    void formatHDEL(const std::string& key, const std::string& field)
    {
        return format("HDEL %s %s", key.c_str(), field.c_str());
    }
    
    const char *c_str() const
    {
        return temp;
    }
    
    size_t length() const
    {
        return strlen(temp);
    }
    
private:
    char *temp;
};

}