#ifndef __DBCONNECTOR__
#define __DBCONNECTOR__

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include <hiredis/hiredis.h>

namespace swss {

class RedisConfig
{
public:
    static bool parseRedisDBConfig(std::string file);
    // updateDBConfigFile is only for vs test / swsscommon test whose db config file doesn't exist
    static bool updateDBConfigFile(std::string file) { m_init = RedisConfig::parseRedisDBConfig(file); return m_init; };
    static std::string getDbinst(int dbId) { return m_db_info[dbId].first; };
    static std::string getDbname(int dbId) { return m_db_info[dbId].second; };
    static std::string getDbsock(int dbId) { return m_redis_info[m_db_info[dbId].first].first; };
    static int getDbport(int dbId) { return m_redis_info[m_db_info[dbId].first].second; };
    static bool isInit() { return m_init; };

private:
    // { instName, {socker, port} }
    static std::unordered_map<std::string, std::pair<std::string, int>> m_redis_info;
    // { dbId, {instName, dbName} }
    static std::unordered_map<int, std::pair<std::string, std::string>> m_db_info;
    static bool m_init;
};

class DBConnector
{
public:
    static constexpr const char *DEFAULT_UNIXSOCKET = "/var/run/redis/redis.sock";

    /*
     * Connect to Redis DB wither with a hostname:port or unix socket
     * Select the database index provided by "db"
     *
     * Timeout - The time in milisecond until exception is been thrown. For
     *           infinite wait, set this value to 0
     */
    DBConnector(int dbId, const std::string &hostname, int port, unsigned int timeout);
    DBConnector(int dbId, const std::string &unixPath, unsigned int timeout);
    DBConnector(const std::string &hostname, int dbId, unsigned int timeout); //TCP PORT
    DBConnector(int dbId, unsigned int timeout); //UNIX SOCKET

    ~DBConnector();

    redisContext *getContext() const;
    int getDbId() const;

    static void select(DBConnector *db);

    /* Create new context to DB */
    DBConnector *newConnector(unsigned int timeout) const;

private:
    redisContext *m_conn;
    int m_dbId;
};

}
#endif
