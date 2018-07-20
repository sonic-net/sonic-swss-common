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

        bool exists(const std::string &key);

        int64_t hdel(const std::string &key, const std::string &field);

        std::unordered_map<std::string, std::string> hgetall(const std::string &key);

        template <typename OutputIterator>
        void hgetall(const std::string &key, OutputIterator result);

        std::vector<std::string> keys(const std::string &key);

        std::vector<std::string> hkeys(const std::string &key);

        void set(const std::string &key, const std::string &value);

        void hset(const std::string &key, const std::string &field, const std::string &value);

        void mset(const std::unordered_map<std::string, std::string> &map);

        template<typename InputIterator>
        void hmset(const std::string &key, InputIterator start, InputIterator stop);

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

template<typename OutputIterator>
void RedisClient::hgetall(const std::string &key, OutputIterator result)
{
    RedisCommand sincr;
    sincr.format("HGETALL %s", key.c_str());
    RedisReply r(m_db, sincr, REDIS_REPLY_ARRAY);

    auto ctx = r.getContext();

    for (unsigned int i = 0; i < ctx->elements; i += 2)
    {
        *result = std::make_pair(ctx->element[i]->str, ctx->element[i+1]->str);
        ++result;
    }
}

template<typename InputIterator>
void RedisClient::hmset(const std::string &key, InputIterator start, InputIterator stop)
{
    RedisCommand shmset;
    shmset.formatHMSET(key, start, stop);
    RedisReply r(m_db, shmset, REDIS_REPLY_STATUS);
}

}

#endif // __REDISCLIENT_H__
