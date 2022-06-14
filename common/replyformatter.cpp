#include <string.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <system_error>
#include <functional>

#include "common/logger.h"
#include "common/replyformatter.h"

using namespace std;

namespace swss {

string to_string(redisReply *reply, string command)
{
    switch(reply->type)
    {
    case REDIS_REPLY_INTEGER:
        return format_reply(command, reply->integer);

    case REDIS_REPLY_STRING:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_NIL:
        return format_reply(command, reply->str, reply->len);

    case REDIS_REPLY_ARRAY:
    {
        return format_reply(command, reply->element, reply->elements);
    }

    default:
        SWSS_LOG_ERROR("invalid type %d for message", reply->type);
        return string();
    }
}

string format_reply(string command, long long integer)
{
    if (command == "COPY"
        || command == "EXPIRE"
        || command == "EXPIREAT"
        || command == "PEXPIRE"
        || command == "PEXPIREAT"
        || command == "HEXISTS"
        || command == "MOVE"
        || command == "MSETNX"
        || command == "PERSIST"
        || command == "RENAMENX"
        || command == "SISMEMBER"
        || command == "SMOVE"
        || command == "SETNX")
    {
        if (integer == 1)
        {
            return string("True");
        }
        else if (integer == 0)
        {
            return string("False");
        }
    }
    else if (command == "AUTH")
    {
        if (integer != 0)
        {
            return string("OK");
        }
    }

    return std::to_string(integer);
}

string format_reply(string command, const char* str, size_t len)
{
    string result = string(str, len);
    if (command == "AUTH"
        || command == "HMSET"
        || command == "PSETEX"
        || command == "SETEX"
        || command == "FLUSHALL"
        || command == "FLUSHDB"
        || command == "LSET"
        || command == "LTRIM"
        || command == "MSET"
        || command == "PFMERGE"
        || command == "ASKING"
        || command == "READONLY"
        || command == "READWRITE"
        || command == "RENAME"
        || command == "SAVE"
        || command == "SELECT"
        || command == "SHUTDOWN"
        || command == "SLAVEOF"
        || command == "SWAPDB"
        || command == "WATCH"
        || command == "UNWATCH"
        || command == "SET")
    {
        if (result == "OK")
        {
            return string("True");
        }
    }

    return result;
}

string format_reply(string command, struct redisReply **element, size_t elements)
{
    if (command == "HGETALL")
    {
        return format_dict_reply(element, elements);
    }
    else if(command == "SCAN"
            || command == "SSCAN")
    {
        return format_sscan_reply(element, elements);
    }
    else if(command == "HSCAN")
    {
        return format_hscan_reply(element, elements);
    }
    else if(command == "BLPOP"
            || command == "BRPOP")
    {
        return format_tuple_reply(element, elements);
    }
    else
    {
        return format_list_reply(element, elements);
    }
}

string format_sscan_reply(struct redisReply **element, size_t elements)
{
    if (elements != 2)
    {
        throw system_error(make_error_code(errc::io_error),
                           "Invalid result");
    }

    // format HSCAN result, here is a example:
    //  (0, {'test3': 'test3', 'test2': 'test2'})
    stringstream result;
    result << "(" << element[0]->integer << ", ";
    // format the field mapping part
    result << format_array_reply(element[1]->element, element[1]->elements);
    result << ")";

    return result.str();
}

string format_hscan_reply(struct redisReply **element, size_t elements)
{
    if (elements != 2)
    {
        throw system_error(make_error_code(errc::io_error),
                           "Invalid result");
    }

    // format HSCAN result, here is a example:
    //  (0, {'test3': 'test3', 'test2': 'test2'})
    stringstream result;
    result << "(" << element[0]->integer << ", ";
    // format the field mapping part
    result << format_dict_reply(element[1]->element, element[1]->elements);
    result << ")";

    return result.str();
}

string format_dict_reply(struct redisReply **element, size_t elements)
{
    if (elements%2 != 0)
    {
        throw system_error(make_error_code(errc::io_error),
                           "Invalid result");
    }

    // format dictionary, not using json.h because the output format are different, here is a example:
    //      {'test3': 'test3', 'test2': 'test2'}
    stringstream result;
    result << "{";

    for (unsigned int i = 0; i < elements; i += 2)
    {
        result << "'" << to_string(element[i]) << "': '" << to_string(element[i+1]) << "'";
        if (i < elements - 2)
        {
            result << ", ";
        }
    }

    result << "}";
    return result.str();
}

string format_array_reply(struct redisReply **element, size_t elements)
{
    stringstream result;
    result << "[";

    for (unsigned int i = 0; i < elements; i++)
    {
        result << "'" << to_string(element[i]) << "'";
        if (i < elements - 1)
        {
            result << ", ";
        }
    }

    result << "]";

    return result.str();
}

string format_list_reply(struct redisReply **element, size_t elements)
{
    stringstream result;
    for (size_t i = 0; i < elements; i++)
    {
        result << to_string(element[i]);
        if (i < elements - 1)
        {
            result << endl;
        }
    }

    return result.str();
}

string format_tuple_reply(struct redisReply **element, size_t elements)
{
    stringstream result;
    result << "(";

    for (unsigned int i = 0; i < elements; i++)
    {
        result << "'" << to_string(element[i]) << "'";
        if (i < elements - 1)
        {
            result << ", ";
        }
    }

    result << ")";

    return result.str();
}

}
