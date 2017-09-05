#include <string.h>
#include <stdint.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <system_error>

#include "common/logger.h"
#include "common/dbconnector.h"
#include "common/redisreply.h"

using namespace std;

namespace swss {

constexpr const char *DBConnector::DEFAULT_UNIXSOCKET;

void DBConnector::select(DBConnector *db)
{
    string select("SELECT ");
    select += to_string(db->getDB());

    RedisReply r(db, select, REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

DBConnector::~DBConnector()
{
    redisFree(m_conn);
}

DBConnector::DBConnector(int db, string hostname, int port,
                         unsigned int timeout) :
    m_db(db)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
        m_conn = redisConnectWithTimeout(hostname.c_str(), port, tv);
    else
        m_conn = redisConnect(hostname.c_str(), port);

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis");

    select(this);
}

DBConnector::DBConnector(int db, string unixPath, unsigned int timeout) :
    m_db(db)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
        m_conn = redisConnectUnixWithTimeout(unixPath.c_str(), tv);
    else
        m_conn = redisConnectUnix(unixPath.c_str());

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis (unixs-socket)");

    select(this);
}

/* No timeout argument, non-blocking DB connection */
DBConnector::DBConnector(int db, string unixPath) :
    m_db(db)
{
    m_conn = redisConnectUnixNonBlock(unixPath.c_str());

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis (non block unix-socket)");
    SWSS_LOG_DEBUG("DBConnector db %d, m_conn->fd %d", db, m_conn->fd);
    m_blocking = false;
}

bool DBConnector::isBlocking()
{
    return m_blocking;
}

redisContext *DBConnector::getContext()
{
    return m_conn;
}

int DBConnector::getDB()
{
    return m_db;
}

DBConnector *DBConnector::newConnector(unsigned int timeout)
{
    if (getContext()->connection_type == REDIS_CONN_TCP)
        return new DBConnector(getDB(),
                               getContext()->tcp.host,
                               getContext()->tcp.port,
                               timeout);
    else
        return new DBConnector(getDB(),
                               getContext()->unix_sock.path,
                               timeout);
}

/* Create Non-blocking DB connection */
DBConnector *DBConnector::newConnector(void)
{
    if (getContext()->connection_type != REDIS_CONN_TCP)
        return new DBConnector(getDB(),
                               getContext()->unix_sock.path);
    else
        return NULL;
}

}
