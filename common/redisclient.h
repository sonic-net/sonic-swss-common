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

#include "dbconnector.h"
#include "redisreply.h"

namespace swss
{

class RedisClient
{
    public:

        RedisClient(swss::DBConnector *db);

        int64_t del(const std::string &key);

        int64_t hdel(const std::string &key, const std::string &field);

        std::unordered_map<std::string, std::string> hgetall(const std::string &key);
        std::map<std::string, std::string> hgetallordered(const std::string &key);

        std::vector<std::string> keys(const std::string &key);

        std::vector<std::string> hkeys(const std::string &key);

        void set(const std::string &key, const std::string &value);

        void hset(const std::string &key, const std::string &field, const std::string &value);

        void mset(const std::unordered_map<std::string, std::string> &map);

        void hmset(const std::string &key, const std::unordered_map<std::string, std::string> &map);
        void hmset(const std::string &key, const std::vector<FieldValueTuple> &values);
        void hmset(const std::string &key, const std::map<std::string, std::string> &vmap);

        std::shared_ptr<std::string> get(const std::string &key);

        std::shared_ptr<std::string> hget(const std::string &key, const std::string &field);

        std::vector<std::shared_ptr<std::string>> mget(const std::vector<std::string> &keys);

        std::vector<std::shared_ptr<std::string>> hmget(const std::string &key, const std::vector<std::string> &fields);

        int64_t incr(const std::string &key);

        int64_t decr(const std::string &key);

        int64_t rpush(const std::string &list, const std::string &item);

        std::shared_ptr<std::string> blpop(const std::string &list, int timeout);

    private:
        swss::DBConnector *m_db;
};

}

#endif // __REDISCLIENT_H__
