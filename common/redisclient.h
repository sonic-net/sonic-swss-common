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

        RedisClient(swss::DBConnector *db) : m_db(db) { }

        int64_t del(const std::string &key) { return m_db->del(key); }

        bool exists(const std::string &key) { return m_db->exists(key); }

        int64_t hdel(const std::string &key, const std::string &field) { return m_db->hdel(key, field); }

        int64_t hdel(const std::string &key, const std::vector<std::string> &fields) { return m_db->hdel(key, fields); }

        std::unordered_map<std::string, std::string> hgetall(const std::string &key) { return m_db->hgetall(key); }

        template <typename OutputIterator>
        void hgetall(const std::string &key, OutputIterator result) { return m_db->hgetall(key, result); }

        std::vector<std::string> keys(const std::string &key) { return m_db->keys(key); }

        bool set(const std::string &key, const std::string &value) { return m_db->set(key, value); }

        void hset(const std::string &key, const std::string &field, const std::string &value) { return m_db->hset(key, field, value); }

        template<typename InputIterator>
        void hmset(const std::string &key, InputIterator start, InputIterator stop) { return m_db->hmset(key, start, stop); }

        std::shared_ptr<std::string> get(const std::string &key) { return m_db->get(key); }

        std::shared_ptr<std::string> hget(const std::string &key, const std::string &field) { return m_db->hget(key, field); }

        int64_t incr(const std::string &key) { return m_db->incr(key); }

        int64_t decr(const std::string &key) { return m_db->decr(key); }

        int64_t rpush(const std::string &list, const std::string &item) { return m_db->rpush(list, item); }

        std::shared_ptr<std::string> blpop(const std::string &list, int timeout) { return m_db->blpop(list, timeout); }

    private:
        swss::DBConnector *m_db;
} __attribute__ ((__deprecated__));
// This class is deprecated. Please use DBConnector class instead.

}

#endif // __REDISCLIENT_H__
