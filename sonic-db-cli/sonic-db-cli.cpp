#include <iostream>
#include "sonic-db-cli.h"

using namespace swss;
using namespace std;

static string g_netns;
static string g_dbOrOperation;
const char* emptyStr = "";

void printUsage(const po::options_description &allOptions)
{
    cout << "usage: sonic-db-cli [-h] [-s] [-n NAMESPACE] db_or_op [cmd ...]" << endl;
    cout << allOptions << endl;
    
    cout << "**sudo** needed for commands accesing a different namespace [-n], or using unixsocket connection [-s]" << endl;
    cout << "" << endl;
    cout << "Example 1: sonic-db-cli -n asic0 CONFIG_DB keys \\*" << endl;
    cout << "Example 2: sonic-db-cli -n asic2 APPL_DB HGETALL VLAN_TABLE:Vlan10" << endl;
    cout << "Example 3: sonic-db-cli APPL_DB HGET VLAN_TABLE:Vlan10 mtu" << endl;
    cout << "Example 4: sonic-db-cli -n asic3 APPL_DB EVAL \"return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}\" 2 k1 k2 v1 v2" << endl;
    cout << "Example 5: sonic-db-cli PING | sonic-db-cli -s PING" << endl;
    cout << "Example 6: sonic-db-cli SAVE | sonic-db-cli -s SAVE" << endl;
    cout << "Example 7: sonic-db-cli FLUSHALL | sonic-db-cli -s FLUSHALL" << endl;
}

void printRedisReply(redisReply* reply)
{
    switch(reply->type)
    {
    case REDIS_REPLY_INTEGER:
        cout << reply->integer;
        break;
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_NIL:
        cout << string(reply->str, reply->len);
        break;
    case REDIS_REPLY_ARRAY: 
        for (size_t i = 0; i < reply->elements; i++)
        {
            printRedisReply(reply->element[i]);
        }
        break;
    default:
        cerr << reply->type << endl;
        throw runtime_error("Unexpected reply type");
    }

    cout << endl;
}

string buildRedisOperation(vector<string>& commands)
{
    int argc = (int)commands.size();
    const char** argv = new const char*[argc];
    size_t* argvc = new size_t[argc];
    for (int i = 0; i < argc; i++)
    {
        argv[i] = strdup(commands[i].c_str());
        argvc[i] = commands[i].size();
    }

    RedisCommand command;
    command.formatArgv(argc, argv, argvc);

    for (int i = 0; i < argc; i++)
    {
        free(const_cast<char*>(argv[i]));
    }
    delete[] argv;
    delete[] argvc;

    return string(command.c_str());
}

int connectDbInterface(
    DBInterface& dbintf,
    const string& db_name,
    const string& netns,
    bool isTcpConn)
{
    try
    {
        if (isTcpConn)
        {
            auto db_hostname = SonicDBConfig::getDbHostname(db_name, netns);
            auto db_port = SonicDBConfig::getDbPort(db_name, netns);
            dbintf.set_redis_kwargs("", db_hostname, db_port);
        }
        else
        {
            auto db_socket = SonicDBConfig::getDbSock(db_name);
            dbintf.set_redis_kwargs(db_socket, "", 0);
        }

        int db_id =  SonicDBConfig::getDbId(db_name, netns);
        dbintf.connect(db_id, db_name, true);
    }
    catch (const exception& e)
    {
        cerr << "Invalid database name input: " << db_name << endl;
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}

int handleSingleOperation(
    const string& netns,
    const string& db_name,
    const string& operation,
    bool isTcpConn)
{
    DBInterface dbintf;
    // Need connect DBInterface first then get redis client, because redis client is data member of DBInterface
    if (connectDbInterface(dbintf, db_name, netns, isTcpConn) != 0)
    {
        return 1;
    }
    auto& client = dbintf.get_redis_client(db_name);

    RedisReply reply(&client, operation);
    auto replyContext = reply.getContext();
    printRedisReply(replyContext);

    return 0;
}

int executeCommands(
    const string& db_name,
    vector<string>& commands,
    const string& netns,
    bool isTcpConn)
{
    auto operation = buildRedisOperation(commands);
    return handleSingleOperation(db_name, operation, netns, isTcpConn);
}

int handleAllInstances(
    const string& netns,
    const string& operation,
    bool isTcpConn)
{
    auto db_names = SonicDBConfig::getDbList(netns);
    for (auto& db_name : db_names)
    {
        int result = handleSingleOperation(netns, db_name, operation, isTcpConn);
        if (result != 0)
        {
            // Stop when any operation failed
            return result;
        }
    }

    return 0;
}

int handleOperation(
    const po::options_description &allOptions,
    const po::variables_map &variablesMap)
{
    if (variablesMap.count("db_or_op"))
    {
        auto dbOrOperation = variablesMap["db_or_op"].as<string>();
        auto netns = variablesMap["--namespace"].as<string>();
        bool isTcpConn = variablesMap.count("--unixsocket") == 0;
        if (netns != "None")
        {
            SonicDBConfig::initializeGlobalConfig(netns);
        }
        else
        {
            // Use the tcp connectivity if namespace is local and unixsocket cmd_option is present.
            isTcpConn = true;
        }
        
        if (variablesMap.count("cmd"))
        {
            auto commands = variablesMap["cmd"].as< vector<string> >();
            return executeCommands(dbOrOperation, commands, netns, isTcpConn);
        }
        else if (netns.compare("PING") == 0 || netns.compare("SAVE") == 0 || netns.compare("FLUSHALL") == 0)
        {
            // redis-cli doesn't depend on database_config.json which could raise some exceptions
            // sonic-db-cli catch all possible exceptions and handle it as a failure case which not return 'OK' or 'PONG'
            return handleAllInstances(netns, dbOrOperation, isTcpConn);
        }
        else
        {
            printUsage(allOptions);
        }
    }
    else
    {
        printUsage(allOptions);
    }

    return 0;
}

void parseCliArguments(
    int argc,
    char** argv,
    po::options_description &allOptions,
    po::variables_map &variablesMap)
{
    allOptions.add_options()
        ("--help,-h", "Help message")
        ("--unixsocket,-s", "Override use of tcp_port and use unixsocket")
        ("--namespace,-n", po::value<string>(&g_netns)->default_value("None"), "Namespace string to use asic0/asic1.../asicn")
        ("db_or_op", po::value<string>(&g_dbOrOperation)->default_value(emptyStr), "Database name Or Unary operation(only PING/SAVE/FLUSHALL supported)")
        ("cmd", po::value< vector<string> >(), "Command to execute in database")
    ;

    po::positional_options_description positional_opts;
    positional_opts.add("db_or_op", 1);
    positional_opts.add("cmd", -1);

    po::store(po::command_line_parser(argc, argv)
                 .options(allOptions)
                 .positional(positional_opts)
                 .run(),
             variablesMap);
    po::notify(variablesMap);
}

#ifdef TESTING
int sonic_db_cli(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    po::options_description allOptions("SONiC DB CLI");
    po::variables_map variablesMap;

    try
    {
        parseCliArguments(argc, argv, allOptions, variablesMap);
    }
    catch (po::error_with_option_name& e)
    {
        cerr << "Command Line Syntax Error: " << e.what() << endl;
        printUsage(allOptions);
        return -1;
    }
    catch (po::error& e)
    {
        cerr << "Command Line Error: " << e.what() << endl;
        printUsage(allOptions);
        return -1;
    }

    if (variablesMap.count("--help"))
    {
        printUsage(allOptions);
        return 0;
    }

    try
    {
        return handleOperation(allOptions, variablesMap);
    }
    catch (const exception& e)
    {
        cerr << "An exception of type " << e.what() << " occurred. Arguments:" << endl;
        for (int idx = 0; idx < argc; idx++)
        {
            cerr << argv[idx]  << " ";
        }
        cerr << endl;
        return 1;
    }

    return 0;
}