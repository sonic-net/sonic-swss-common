
#ifndef __REPLYFORMATTER__
#define __REPLYFORMATTER__

#include "redisreply.h"

/*
    Response format need keep as same as redis-py, redis-py using a command to result type mapping to convert result:
        https://github.com/redis/redis-py/blob/bedf3c82a55b4b67eed93f686cb17e82f7ab19cd/redis/client.py#L682
    Redis command result type can be found here:
        https://redis.io/commands/
    Only commands used by scripts in sonic repos are supported by these method.
    For example: 'Info' command not used by any sonic scripts, so the output format will different with redis-py.
*/

namespace swss {

std::string to_string(redisReply *reply, std::string command = std::string());

std::string format_reply(std::string command, struct redisReply **element, size_t elements);

std::string format_reply(std::string command, long long integer);

std::string format_reply(std::string command, const char* str, size_t len);

std::string format_sscan_reply(struct redisReply **element, size_t elements);

std::string format_hscan_reply(struct redisReply **element, size_t elements);

std::string format_dict_reply(struct redisReply **element, size_t elements);

std::string format_array_reply(struct redisReply **element, size_t elements);

std::string format_list_reply(struct redisReply **element, size_t elements);

std::string format_tuple_reply(struct redisReply **element, size_t elements);

}

#endif
