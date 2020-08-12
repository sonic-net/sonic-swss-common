#include <string.h>
#include <stdint.h>
#include <vector>
#include <unistd.h>
#include <errno.h>
#include <system_error>
#include <fstream>
#include "json.hpp"
#include "logger.h"

#include "common/dbconnector.h"
#include "common/redisreply.h"

using json = nlohmann::json;
using namespace std;

namespace swss {

void SonicDBConfig::initialize(const string &file)
{

    SWSS_LOG_ENTER();

    if (m_init)
    {
        SWSS_LOG_ERROR("SonicDBConfig already initialized");
        throw runtime_error("SonicDBConfig already initialized");
    }

    ifstream i(file);
    if (i.good())
    {
        try
        {
            json j;
            i >> j;
            for (auto it = j["INSTANCES"].begin(); it!= j["INSTANCES"].end(); it++)
            {
               string instName = it.key();
               string socket = it.value().at("unix_socket_path");
               string hostname = it.value().at("hostname");
               int port = it.value().at("port");
               m_inst_info[instName] = {socket, {hostname, port}};
            }

            for (auto it = j["DATABASES"].begin(); it!= j["DATABASES"].end(); it++)
            {
               string dbName = it.key();
               string instName = it.value().at("instance");
               int dbId = it.value().at("id");
               string separator = it.value().at("separator");
               m_db_info[dbName] = {instName, dbId, separator};

               m_db_separator.emplace(dbId, separator);
            }
            m_init = true;
        }
        catch (domain_error& e)
        {
            SWSS_LOG_ERROR("key doesn't exist in json object, NULL value has no iterator >> %s\n", e.what());
            throw runtime_error("key doesn't exist in json object, NULL value has no iterator >> " + string(e.what()));
        }
        catch (exception &e)
        {
            SWSS_LOG_ERROR("Sonic database config file syntax error >> %s\n", e.what());
            throw runtime_error("Sonic database config file syntax error >> " + string(e.what()));
        }
    }
    else
    {
        SWSS_LOG_ERROR("Sonic database config file doesn't exist at %s\n", file.c_str());
        throw runtime_error("Sonic database config file doesn't exist at " + file);
    }
}

string SonicDBConfig::getDbInst(const string &dbName)
{
    if (!m_init)
        initialize();
    return m_db_info.at(dbName).instName;
}

int SonicDBConfig::getDbId(const string &dbName)
{
    if (!m_init)
        initialize();
    return m_db_info.at(dbName).dbId;
}

string SonicDBConfig::getSeparator(const string &dbName)
{
    if (!m_init)
        initialize();
    return m_db_info.at(dbName).separator;
}

string SonicDBConfig::getSeparator(int dbId)
{
    if (!m_init)
        initialize();
    return m_db_separator.at(dbId);
}

string SonicDBConfig::getSeparator(const DBConnector* db)
{
    if (!db)
    {
        throw std::invalid_argument("db cannot be null");
    }

    string dbName = db->getDbName();
    if (dbName.empty())
    {
        return getSeparator(db->getDbId());
    }
    else
    {
        return getSeparator(dbName);
    }
}

string SonicDBConfig::getDbSock(const string &dbName)
{
    if (!m_init)
        initialize();
    return m_inst_info.at(getDbInst(dbName)).first;
}

string SonicDBConfig::getDbHostname(const string &dbName)
{
    if (!m_init)
        initialize();
    return m_inst_info.at(getDbInst(dbName)).second.first;
}

int SonicDBConfig::getDbPort(const string &dbName)
{
    if (!m_init)
        initialize();
    return m_inst_info.at(getDbInst(dbName)).second.second;
}

constexpr const char *SonicDBConfig::DEFAULT_SONIC_DB_CONFIG_FILE;
unordered_map<string, pair<string, pair<string, int>>> SonicDBConfig::m_inst_info;
unordered_map<string, SonicDBInfo> SonicDBConfig::m_db_info;
unordered_map<int, string> SonicDBConfig::m_db_separator;
bool SonicDBConfig::m_init = false;

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

DBConnector::DBConnector(const string& dbName, unsigned int timeout, bool isTcpConn)
    : m_dbId(SonicDBConfig::getDbId(dbName))
    , m_dbName(dbName)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
    {
        if (isTcpConn)
            m_conn = redisConnectWithTimeout(SonicDBConfig::getDbHostname(dbName).c_str(), SonicDBConfig::getDbPort(dbName), tv);
        else
            m_conn = redisConnectUnixWithTimeout(SonicDBConfig::getDbSock(dbName).c_str(), tv);
    }
    else
    {
        if (isTcpConn)
            m_conn = redisConnect(SonicDBConfig::getDbHostname(dbName).c_str(), SonicDBConfig::getDbPort(dbName));
        else
            m_conn = redisConnectUnix(SonicDBConfig::getDbSock(dbName).c_str());
    }

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis");

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

string DBConnector::getDbName() const
{
    return m_dbName;
}

DBConnector *DBConnector::newConnector(unsigned int timeout) const
{
    DBConnector *ret;
    if (getContext()->connection_type == REDIS_CONN_TCP)
        ret = new DBConnector(getDbId(),
                               getContext()->tcp.host,
                               getContext()->tcp.port,
                               timeout);
    else
        ret = new DBConnector(getDbId(),
                               getContext()->unix_sock.path,
                               timeout);
    ret->m_dbName = m_dbName;
    return ret;
}

void DBConnector::setClientName(const string& clientName)
{
    string command("CLIENT SETNAME ");
    command += clientName;

    RedisReply r(this, command, REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

string DBConnector::getClientName()
{
    string command("CLIENT GETNAME");

    RedisReply r(this, command);

    auto ctx = r.getContext();
    if (ctx->type == REDIS_REPLY_STRING)
    {
        return r.getReply<std::string>();
    }
    else
    {
        if (ctx->type != REDIS_REPLY_NIL)
            SWSS_LOG_ERROR("Unable to obtain Redis client name");

        return "";
    }
}

}
