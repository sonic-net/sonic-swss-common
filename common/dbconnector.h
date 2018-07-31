#ifndef __DBCONNECTOR__
#define __DBCONNECTOR__

#include <string>
#include <vector>

#include <hiredis/hiredis.h>

namespace swss {

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
