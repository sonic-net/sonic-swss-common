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

void SonicDBConfig::parseDatabaseConfig(const string &file,
                    std::unordered_map<std::string, RedisInstInfo> &inst_entry,
                    std::unordered_map<std::string, SonicDBInfo> &db_entry,
                    std::unordered_map<int, std::string> &separator_entry)
{
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
               inst_entry[instName] = {socket, hostname, port};
            }

            for (auto it = j["DATABASES"].begin(); it!= j["DATABASES"].end(); it++)
            {
               string dbName = it.key();
               string instName = it.value().at("instance");
               int dbId = it.value().at("id");
               string separator = it.value().at("separator");
               db_entry[dbName] = {instName, dbId, separator};

               separator_entry.emplace(dbId, separator);
            }
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

void SonicDBConfig::initializeGlobalConfig(const string &file)
{
    std::string local_file, dir_name, ns_name;
    std::unordered_map<std::string, SonicDBInfo> db_entry;
    std::unordered_map<std::string, RedisInstInfo> inst_entry;
    std::unordered_map<int, std::string> separator_entry;

    SWSS_LOG_ENTER();

    if (m_global_init)
    {
        SWSS_LOG_ERROR("SonicDBConfig Global config is already initialized");
        return;
    }

    ifstream i(file);
    if (i.good())
    {
        local_file = dir_name = std::string();

        // Get the directory name from the file path given as input.
        std::string::size_type pos = file.rfind("/");
        if( pos != std::string::npos)
        {
            dir_name = file.substr(0,pos+1);
        }

        try
        {
            json j;
            i >> j;

            for (auto& element : j["INCLUDES"])
            {
                local_file.append(dir_name);
                local_file.append(element["include"]);

                if(element["namespace"].empty())
                {
                    // If database_config.json is already initlized via SonicDBConfig::initialize
                    // skip initializing it here again.
                    if(m_init)
                    {
                        local_file.clear();
                        continue;
                    }
                    ns_name = EMPTY_NAMESPACE;
                }
                else
                {
                    ns_name = element["namespace"];
                }

                parseDatabaseConfig(local_file, inst_entry, db_entry, separator_entry);
                m_inst_info[ns_name] = inst_entry;
                m_db_info[ns_name] = db_entry;
                m_db_separator[ns_name] = separator_entry;

                if(element["namespace"].empty())
                {
                    // Make regular init also done
                    m_init = true;
                }

                inst_entry.clear();
                db_entry.clear();
                separator_entry.clear();
                local_file.clear();
            }
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
        SWSS_LOG_ERROR("Sonic database config global file doesn't exist at %s\n", file.c_str());
    }

    // Set it as the global config file is already parsed and init done.
    m_global_init = true;
}

void SonicDBConfig::initialize(const string &file)
{
    std::unordered_map<std::string, SonicDBInfo> db_entry;
    std::unordered_map<std::string, RedisInstInfo> inst_entry;
    std::unordered_map<int, std::string> separator_entry;

    SWSS_LOG_ENTER();

    if (m_init)
    {
        SWSS_LOG_ERROR("SonicDBConfig already initialized");
        throw runtime_error("SonicDBConfig already initialized");
    }

    parseDatabaseConfig(file, inst_entry, db_entry, separator_entry);
    m_inst_info[EMPTY_NAMESPACE] = inst_entry;
    m_db_info[EMPTY_NAMESPACE] = db_entry;
    m_db_separator[EMPTY_NAMESPACE] = separator_entry;

    // Set it as the config file is already parsed and init done.
    m_init = true;
}

void SonicDBConfig::validateNamespace(const string &nameSpace)
{
    SWSS_LOG_ENTER();

    // With valid namespace input and database_global.json is not loaded, ask user to initializeGlobalConfig first
    if(!nameSpace.empty())
    {
        // If global initialization is not done, ask user to initialize global DB Config first.
        if (!m_global_init)
        {
            SWSS_LOG_ERROR("Initialize global DB config first using API SonicDBConfig::initializeGlobalConfig \n");
            throw runtime_error("Initialize global DB config using API SonicDBConfig::initializeGlobalConfig");
        }

        // Check if the namespace is valid, check if this is a key in either of this map
        unordered_map<string, unordered_map<string, RedisInstInfo>>::const_iterator entry = m_inst_info.find(nameSpace);
        if (entry == m_inst_info.end())
        {
            SWSS_LOG_ERROR("Namespace %s is not a valid namespace name in config file\n", nameSpace.c_str());
            throw runtime_error("Namespace " + nameSpace + " is not a valid namespace name in config file");
        }
    }
}

string SonicDBConfig::getDbInst(const string &dbName, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_db_info[nameSpace].at(dbName).instName;
}

int SonicDBConfig::getDbId(const string &dbName, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_db_info[nameSpace].at(dbName).dbId;
}

string SonicDBConfig::getSeparator(const string &dbName, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_db_info[nameSpace].at(dbName).separator;
}

string SonicDBConfig::getSeparator(int dbId, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_db_separator[nameSpace].at(dbId);
}

string SonicDBConfig::getSeparator(const DBConnector* db)
{
    if (!db)
    {
        throw std::invalid_argument("db cannot be null");
    }

    string dbName = db->getDbName();
    string nameSpace = db->getNamespace();
    if (dbName.empty())
    {
        return getSeparator(db->getDbId(), nameSpace);
    }
    else
    {
        return getSeparator(dbName, nameSpace);
    }
}

string SonicDBConfig::getDbSock(const string &dbName, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_inst_info[nameSpace].at(getDbInst(dbName)).unixSocketPath;
}

string SonicDBConfig::getDbHostname(const string &dbName, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_inst_info[nameSpace].at(getDbInst(dbName)).hostname;
}

int SonicDBConfig::getDbPort(const string &dbName, const string &nameSpace)
{
    if (!m_init)
        initialize(DEFAULT_SONIC_DB_CONFIG_FILE);
    validateNamespace(nameSpace);
    return m_inst_info[nameSpace].at(getDbInst(dbName)).port;
}

vector<string> SonicDBConfig::getNamespaces()
{
    vector<string> list;

    if (!m_global_init)
        initializeGlobalConfig();

    // This API returns back non-empty namespaces.
    for (auto it = m_inst_info.cbegin(); it != m_inst_info.cend(); ++it) {
        if(!((it->first).empty()))
            list.push_back(it->first);
    }

    return list;
}

constexpr const char *SonicDBConfig::DEFAULT_SONIC_DB_CONFIG_FILE;
constexpr const char *SonicDBConfig::DEFAULT_SONIC_DB_GLOBAL_CONFIG_FILE;
unordered_map<string, unordered_map<string, RedisInstInfo>> SonicDBConfig::m_inst_info;
unordered_map<string, unordered_map<string, SonicDBInfo>> SonicDBConfig::m_db_info;
unordered_map<string, unordered_map<int, string>> SonicDBConfig::m_db_separator;
bool SonicDBConfig::m_init = false;
bool SonicDBConfig::m_global_init = false;

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
    m_dbId(dbId),
    m_namespace(EMPTY_NAMESPACE)
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
    m_dbId(dbId),
    m_namespace(EMPTY_NAMESPACE)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
        m_conn = redisConnectUnixWithTimeout(unixPath.c_str(), tv);
    else
        m_conn = redisConnectUnix(unixPath.c_str());

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis (unix-socket)");

    select(this);
}

DBConnector::DBConnector(const string& dbName, unsigned int timeout, bool isTcpConn, const string& nameSpace)
    : m_dbId(SonicDBConfig::getDbId(dbName, nameSpace))
    , m_dbName(dbName)
    , m_namespace(nameSpace)
{
    struct timeval tv = {0, (suseconds_t)timeout * 1000};

    if (timeout)
    {
        if (isTcpConn)
            m_conn = redisConnectWithTimeout(SonicDBConfig::getDbHostname(dbName, nameSpace).c_str(), SonicDBConfig::getDbPort(dbName, nameSpace), tv);
        else
            m_conn = redisConnectUnixWithTimeout(SonicDBConfig::getDbSock(dbName, nameSpace).c_str(), tv);
    }
    else
    {
        if (isTcpConn)
            m_conn = redisConnect(SonicDBConfig::getDbHostname(dbName, nameSpace).c_str(), SonicDBConfig::getDbPort(dbName, nameSpace));
        else
            m_conn = redisConnectUnix(SonicDBConfig::getDbSock(dbName, nameSpace).c_str());
    }

    if (m_conn->err)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect to redis");

    select(this);
}

DBConnector::DBConnector(const string& dbName, unsigned int timeout, bool isTcpConn)
    : DBConnector(dbName, timeout, isTcpConn, EMPTY_NAMESPACE)
{
    // Empty contructor
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

string DBConnector::getNamespace() const
{
    return m_namespace;
}

DBConnector *DBConnector::newConnector(unsigned int timeout) const
{
    DBConnector *ret;

    // The DBConnector object created with the class constructors defined before Multi-DB, Multi-NS design,
    // will have only dbId as valid, dbName is NULL.
    if (m_dbName.empty())
    {
        if (getContext()->connection_type == REDIS_CONN_TCP)
            ret = new DBConnector(getDbId(),
                                   getContext()->tcp.host,
                                   getContext()->tcp.port,
                                   timeout);
        else
            ret = new DBConnector(getDbId(),
                                   getContext()->unix_sock.path,
                                   timeout);
    }
    else
    {
        ret = new DBConnector(getDbName(), timeout, (getContext()->connection_type == REDIS_CONN_TCP), getNamespace());
    }

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
