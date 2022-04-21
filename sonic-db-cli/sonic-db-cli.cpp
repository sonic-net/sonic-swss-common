#include <execution>
#include <iostream>

#include <swss/dbconnector.h>
#include <swss/sonicv2connector.h>

#include "sonic-db-cli.h"

static std::string name_space;
static std::string db_or_operation;
const char* empty_str = "";

void printUsage(const po::options_description &all_options)
{
    std::cout << "usage: sonic-db-cli [-h] [-s] [-n NAMESPACE] db_or_op [cmd ...]" << std::endl;
    std::cout << all_options << std::endl;
    
    std::cout << "**sudo** needed for commands accesing a different namespace [-n], or using unixsocket connection [-s]" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Example 1: sonic-db-cli -n asic0 CONFIG_DB keys \\*" << std::endl;
    std::cout << "Example 2: sonic-db-cli -n asic2 APPL_DB HGETALL VLAN_TABLE:Vlan10" << std::endl;
    std::cout << "Example 3: sonic-db-cli APPL_DB HGET VLAN_TABLE:Vlan10 mtu" << std::endl;
    std::cout << "Example 4: sonic-db-cli -n asic3 APPL_DB EVAL \"return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}\" 2 k1 k2 v1 v2" << std::endl;
    std::cout << "Example 5: sonic-db-cli PING | sonic-db-cli -s PING" << std::endl;
    std::cout << "Example 6: sonic-db-cli SAVE | sonic-db-cli -s SAVE" << std::endl;
    std::cout << "Example 7: sonic-db-cli FLUSHALL | sonic-db-cli -s FLUSHALL" << std::endl;
}

void printRedisReply(redisReply* reply)
{
    switch(reply->type)
    {
    case REDIS_REPLY_INTEGER:
        std::cout << reply->integer;
        break;
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_NIL:
        std::cout << std::string(reply->str, reply->len);
        break;
    case REDIS_REPLY_ARRAY: 
        for (size_t i = 0; i < reply->elements; i++)
        {
            printRedisReply(reply->element[i]);
        }
        break;
    default:
        std::cerr << reply->type << std::endl;
        throw std::runtime_error("Unexpected reply type");
    }

    std::cout << std::endl;
}

int executeCommands(
    const std::string& db_name,
    std::vector<std::string>& commands,
    const std::string& name_space,
    bool use_unix_socket)
{
    std::unique_ptr<swss::SonicV2Connector_Native> dbconn;
    if (name_space.compare("None") == 0) {
        dbconn = std::make_unique<swss::SonicV2Connector_Native>(use_unix_socket, empty_str);
    }
    else {
        dbconn = std::make_unique<swss::SonicV2Connector_Native>(true, name_space.c_str());
    }

    try {
        dbconn->connect(db_name);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Invalid database name input: " << db_name << std::endl;
        std::cerr << e.what() << std::endl;
        return 1;
    }

    auto& client = dbconn->get_redis_client(db_name);

    size_t argc = commands.size();
    const char** argv = new const char*[argc];
    size_t* argvc = new size_t[argc];
    for (size_t i = 0; i < argc; i++)
    {
        argv[i] = strdup(commands[i].c_str());
        argvc[i] = commands[i].size();
    }

    swss::RedisCommand command;
    command.formatArgv(argc, argv, argvc);
    swss::RedisReply reply(&client, command);

    auto redisReply = reply.getContext();
    printRedisReply(redisReply);

    return 0;
}

void handleSingleOperation(
    const std::string& name_space,
    const std::string& db_name,
    const std::string& operation,
    bool use_unix_socket)
{
    swss::SonicV2Connector_Native conn(use_unix_socket, name_space.c_str());
    conn.connect(db_name);
    auto& client = conn.get_redis_client(db_name);
    
    swss::RedisReply reply(&client, operation);
    auto redisReply = reply.getContext();
    printRedisReply(redisReply);
}

void handleAllInstances(
    const std::string& name_space,
    const std::string& operation,
    bool use_unix_socket)
{
    // Use the tcp connectivity if namespace is local and unixsocket cmd_option is present.
    if (name_space.compare("None") == 0) {
        use_unix_socket = true;
    }
    
    auto dbNames = swss::SonicDBConfig::getDbList(name_space);
    // Operate All Redis Instances in Parallel
    // TODO: if one of the operations failed, it could fail quickly and not necessary to wait all other operations
    std::for_each(
        std::execution::par,
        dbNames.begin(),
        dbNames.end(),
        [=](auto&& db_name)
        {
            handleSingleOperation(name_space, db_name, operation, use_unix_socket);
        });
}

int handleOperation(
    const po::options_description &all_options,
    const po::variables_map &variables_map)
{
    if (variables_map.count("db_or_op")) {
        auto db_or_operation = variables_map["db_or_op"].as<std::string>();
        auto name_space = variables_map["--namespace"].as<std::string>();
        bool useUnixSocket = variables_map.count("--unixsocket");
        if (name_space.compare("None") != 0) {
            swss::SonicDBConfig::initializeGlobalConfig(name_space);
        }
        
        if (variables_map.count("cmd")) {
            auto commands = variables_map["cmd"].as< std::vector<std::string> >();
            return executeCommands(db_or_operation, commands, name_space, useUnixSocket);
        }
        else if (name_space.compare("PING") == 0 || name_space.compare("SAVE") == 0 || name_space.compare("FLUSHALL") == 0) {
            // redis-cli doesn't depend on database_config.json which could raise some exceptions
            // sonic-db-cli catch all possible exceptions and handle it as a failure case which not return 'OK' or 'PONG'
            handleAllInstances(name_space, db_or_operation, useUnixSocket);
        }
        else {
            printUsage(all_options);
        }
    }
    else {
        printUsage(all_options);
    }

    return 0;
}

void parseCliArguments(
    int argc,
    char** argv,
    po::options_description &all_options,
    po::variables_map &variables_map)
{
    all_options.add_options()
        ("--help,-h", "Help message")
        ("--unixsocket,-s", "Override use of tcp_port and use unixsocket")
        ("--namespace,-n", po::value<std::string>(&name_space)->default_value("None"), "Namespace string to use asic0/asic1.../asicn")
        ("db_or_op", po::value<std::string>(&db_or_operation)->default_value(empty_str), "Database name Or Unary operation(only PING/SAVE/FLUSHALL supported)")
        ("cmd", po::value< std::vector<std::string> >(), "Command to execute in database")
    ;

    po::positional_options_description positional_opts;
    positional_opts.add("db_or_op", 1);
    positional_opts.add("cmd", -1);

    po::store(po::command_line_parser(argc, argv)
                 .options(all_options)
                 .positional(positional_opts)
                 .run(),
             variables_map);
    po::notify(variables_map);
}

#ifdef TESTING
int sonic_db_cli(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    po::options_description all_options("SONiC DB CLI");
    po::variables_map variables_map;

    try {
        parseCliArguments(argc, argv, all_options, variables_map);
    }
    catch (po::error_with_option_name& e) {
        std::cerr << "Command Line Syntax Error: " << e.what() << std::endl;
        printUsage(all_options);
        return -1;
    }
    catch (po::error& e) {
        std::cerr << "Command Line Error: " << e.what() << std::endl;
        printUsage(all_options);
        return -1;
    }

    if (variables_map.count("--help")) {
        printUsage(all_options);
        return 0;
    }

    try
    {
        return handleOperation(all_options, variables_map);
    }
    catch (const std::exception& e)
    {
        std::cerr << "An exception of type " << e.what() << " occurred. Arguments:" << std::endl;
        for (int idx = 0; idx < argc; idx++) {
            std::cerr << argv[idx]  << " ";
        }
        std::cerr << std::endl;
        return 1;
    }

    return 0;
}