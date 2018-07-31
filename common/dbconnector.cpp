#include <string.h>
#include <stdint.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <system_error>

#include "common/dbconnector.h"
#include "common/redisreply.h"

using namespace std;

namespace swss {

constexpr const char *DBConnector::DEFAULT_UNIXSOCKET;

void DBConnector::select(DBConnector *db)
{
    string select("SELECT ");
    select += to_string(db->getDbId());

    RedisReply r(db, select, REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

DBConnector::~DBConnector()
{
    redisFree(m_conn);
}

DBConnector::DBConnector(int dbId, const string& hostname, int port,
                         unsigned int timeout) :
    m_dbId(dbId)
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

DBConnector::DBConnector(int dbId, const string& unixPath, unsigned int timeout) :
    m_dbId(dbId)
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

redisContext *DBConnector::getContext() const
{
    return m_conn;
}

int DBConnector::getDbId() const
{
    return m_dbId;
}

DBConnector *DBConnector::newConnector(unsigned int timeout) const
{
    if (getContext()->connection_type == REDIS_CONN_TCP)
        return new DBConnector(getDbId(),
                               getContext()->tcp.host,
                               getContext()->tcp.port,
                               timeout);
    else
        return new DBConnector(getDbId(),
                               getContext()->unix_sock.path,
                               timeout);
}

}
