#ifndef __DBCONNECTOR__
#define __DBCONNECTOR__

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include <hiredis/hiredis.h>

namespace swss {

class DBConnector;

class SonicDBInfo
{
public:
    std::string instName;
    int dbId;
    std::string separator;
};

class SonicDBConfig
{
public:
    static void initialize(const std::string &file = DEFAULT_SONIC_DB_CONFIG_FILE);
    static std::string getDbInst(const std::string &dbName);
    static int getDbId(const std::string &dbName);
    static std::string getSeparator(const std::string &dbName);
    static std::string getSeparator(int dbId);
    static std::string getSeparator(const DBConnector* db);
    static std::string getDbSock(const std::string &dbName);
    static std::string getDbHostname(const std::string &dbName);
    static int getDbPort(const std::string &dbName);
    static bool isInit() { return m_init; };

private:
    static constexpr const char *DEFAULT_SONIC_DB_CONFIG_FILE = "/var/run/redis/sonic-db/database_config.json";
    // { instName, { unix_socket_path, {hostname, port} } }
    static std::unordered_map<std::string, std::pair<std::string, std::pair<std::string, int>>> m_inst_info;
    // { dbName, {instName, dbId} }
    static std::unordered_map<std::string, SonicDBInfo> m_db_info;
    // { dbIp, separator }
    static std::unordered_map<int, std::string> m_db_separator;
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
    DBConnector(const std::string &dbName, unsigned int timeout, bool isTcpConn = false);

    ~DBConnector();

    redisContext *getContext() const;
    int getDbId() const;
    std::string getDbName() const;

    static void select(DBConnector *db);

    /* Create new context to DB */
    DBConnector *newConnector(unsigned int timeout) const;

private:
    redisContext *m_conn;
    int m_dbId;
    std::string m_dbName;
};

}
#endif
