#ifndef __DBCONNECTOR__
#define __DBCONNECTOR__

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include <hiredis/hiredis.h>
#define EMPTY_NAMESPACE std::string()

namespace swss {

class DBConnector;

class RedisInstInfo
{
public:
    std::string unixSocketPath;
    std::string hostname;
    int port;
};

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
    static void initialize(const std::string &file = DEFAULT_SONIC_DB_CONFIG_FILE, const std::string &nameSpace = EMPTY_NAMESPACE);
    static void initializeGlobalConfig(const std::string &file = DEFAULT_SONIC_DB_GLOBAL_CONFIG_FILE);
    static std::string getDbInst(const std::string &dbName, const std::string &nameSpace = EMPTY_NAMESPACE);
    static int getDbId(const std::string &dbName, const std::string &nameSpace = EMPTY_NAMESPACE);
    static std::string getSeparator(const std::string &dbName, const std::string &nameSpace = EMPTY_NAMESPACE);
    static std::string getSeparator(int dbId, const std::string &nameSpace = EMPTY_NAMESPACE);
    static std::string getSeparator(const DBConnector* db);
    static std::string getDbSock(const std::string &dbName, const std::string &nameSpace = EMPTY_NAMESPACE);
    static std::string getDbHostname(const std::string &dbName, const std::string &nameSpace = EMPTY_NAMESPACE);
    static int getDbPort(const std::string &dbName, const std::string &nameSpace = EMPTY_NAMESPACE);
    static std::vector<std::string> getNamespaces();
    static bool isInit() { return m_init; };
    static bool isGlobalInit() { return m_global_init; };

private:
    static constexpr const char *DEFAULT_SONIC_DB_CONFIG_FILE = "/var/run/redis/sonic-db/database_config.json";
    static constexpr const char *DEFAULT_SONIC_DB_GLOBAL_CONFIG_FILE = "/var/run/redis/sonic-db/database_global.json";
    // { namespace { instName, { unix_socket_path, hostname, port } } }
    static std::unordered_map<std::string, std::unordered_map<std::string, RedisInstInfo>> m_inst_info;
    // { namespace, { dbName, {instName, dbId, separator} } }
    static std::unordered_map<std::string, std::unordered_map<std::string, SonicDBInfo>> m_db_info;
    // { namespace, { dbId, separator } }
    static std::unordered_map<std::string, std::unordered_map<int, std::string>> m_db_separator;
    static bool m_init;
    static bool m_global_init;
    static void parseDatabaseConfig(const std::string &file,
                                    std::unordered_map<std::string, RedisInstInfo> &inst_entry,
                                    std::unordered_map<std::string, SonicDBInfo> &db_entry,
                                    std::unordered_map<int, std::string> &separator_entry);
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
    DBConnector(const std::string &dbName, unsigned int timeout, bool isTcpConn = false, const std::string &nameSpace = EMPTY_NAMESPACE);

    ~DBConnector();

    redisContext *getContext() const;
    int getDbId() const;
    std::string getDbName() const;
    std::string getNamespace() const;

    static void select(DBConnector *db);

    /* Create new context to DB */
    DBConnector *newConnector(unsigned int timeout) const;

    /*
     * Assign a name to the Redis client used for this connection
     * This is helpful when debugging Redis clients using `redis-cli client list`
     */
    void setClientName(const std::string& clientName);

    std::string getClientName();

private:
    redisContext *m_conn;
    int m_dbId;
    std::string m_dbName;
    std::string m_namespace;
};

}
#endif
