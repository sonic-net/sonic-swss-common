#include <string.h>
#include <stdint.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <system_error>
#include <fstream>
#include "json.hpp"
#include "schema.h"

#include "common/dbconnector.h"
#include "common/redisreply.h"

using json = nlohmann::json;
using namespace std;

namespace swss {

bool RedisConfig::parseRedisDBConfig(string file) {

    ifstream i(file);
    if (i.good()) {
        json j;
        i >> j;
        for(auto it = j["INSTANCES"].begin(); it!= j["INSTANCES"].end(); it++) {
           string instName = it.key();
           string sockPath = it.value().at("socket");
           int port = it.value().at("port");
           m_redis_info[instName] = {sockPath, port};
        }

        for(auto it = j["DATABASES"].begin(); it!= j["DATABASES"].end(); it++) {
           string dbName = it.key();
           string instName = it.value().at("instance");
           int dbId = it.value().at("id");
           m_db_info[dbId] = {instName, dbName};
        }
        return true;
    }
    return false;
};

unordered_map<int, pair<string, string>> RedisConfig::m_db_info;
unordered_map<string, pair<string, int>> RedisConfig::m_redis_info;
bool RedisConfig::m_init = RedisConfig::parseRedisDBConfig(DB_CONFIG_FILE);

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

DBConnector::DBConnector(const string& hostname, int dbId, unsigned int timeout) :
    m_dbId(dbId)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
        m_conn = redisConnectWithTimeout(hostname.c_str(), RedisConfig::getDbport(dbId), tv);
    else
        m_conn = redisConnect(hostname.c_str(), RedisConfig::getDbport(dbId));

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis");

    select(this);
}

DBConnector::DBConnector(int dbId, unsigned int timeout) :
   m_dbId(dbId)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
        m_conn = redisConnectUnixWithTimeout(RedisConfig::getDbsock(dbId).c_str(), tv);
    else
        m_conn = redisConnectUnix(RedisConfig::getDbsock(dbId).c_str());

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
