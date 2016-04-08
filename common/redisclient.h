#ifndef __REDISCLIENT_H__
#define __REDISCLIENT_H__

#include <iostream>
#include <string>
#include <vector>
#include <ios>
#include <map>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <memory>

#include "swss/dbconnector.h"
#include "swss/redisreply.h"

namespace swss
{

class RedisClient
{
    public:

        RedisClient(swss::DBConnector *db);

        int64_t del(std::string key);

        int64_t hdel(std::string key, std::string field);

        std::unordered_map<std::string, std::string> hgetall(std::string key);

        std::vector<std::string> keys(std::string key);

        std::vector<std::string> hkeys(std::string key);

        void set(std::string key, std::string value);

        void hset(std::string key, std::string field, std::string value);

        void mset(std::unordered_map<std::string, std::string> map);

        void hmset(std::string key, std::unordered_map<std::string, std::string> map);

        std::shared_ptr<std::string> get(std::string key);

        std::shared_ptr<std::string> hget(std::string key, std::string field);

        std::vector<std::shared_ptr<std::string>> mget(std::vector<std::string> keys);

        std::vector<std::shared_ptr<std::string>> hmget(std::string key, std::vector<std::string> fields);

        int64_t incr(std::string key);

        int64_t decr(std::string key);

        int64_t rpush(std::string list, std::string item);

        std::shared_ptr<std::string> blpop(std::string list, int timeout);

    private:
        swss::DBConnector *m_db;
};

}

#endif // __REDISCLIENT_H__
