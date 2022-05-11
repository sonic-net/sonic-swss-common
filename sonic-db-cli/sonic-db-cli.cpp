#include <future>
#include <iostream>
#include <getopt.h>
#include "sonic-db-cli.h"

using namespace swss;
using namespace std;

void printUsage()
{
    cout << "usage: sonic-db-cli [-h] [-s] [-n NAMESPACE] db_or_op [cmd ...]" << endl;
    cout << "SONiC DB CLI:" << endl;
    cout << "  --help                  Help message" << endl;
    cout << "  --unixsocket            Override use of tcp_port and use unixsocket" << endl;
    cout << "  --namespace arg (=None) Namespace string to use asic0/asic1.../asicn" << endl;
    cout << "  db_or_op            Database name Or Unary operation(only PING/SAVE/FLUSHALL supported)" << endl;
    cout << "  cmd                 Command to execute in database" << endl;
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
    vector<const char*> args;
    for (auto& command : commands)
    {
        args.push_back(command.c_str());
    }

    RedisCommand command;
    command.formatArgv((int)args.size(), args.data(), NULL);

    return string(command.c_str());
}

shared_ptr<DBConnector> connectDbInterface(
    const string& db_name,
    const string& netns,
    bool isTcpConn)
{
    try
    {
        int db_id =  SonicDBConfig::getDbId(db_name, netns);
        if (isTcpConn)
        {
            auto host = SonicDBConfig::getDbHostname(db_name, netns);
            auto port = SonicDBConfig::getDbPort(db_name, netns);
            return make_shared<DBConnector>(db_id, host, port, 0);
        }
        else
        {
            auto db_socket = SonicDBConfig::getDbSock(db_name);
            return make_shared<DBConnector>(db_id, db_socket, 0);
        }
    }
    catch (const exception& e)
    {
        cerr << "Invalid database name input: " << db_name << endl;
        cerr << e.what() << endl;
        return nullptr;
    }
}

int handleSingleOperation(
    const string& netns,
    const string& db_name,
    const string& operation,
    bool isTcpConn)
{
    auto client = connectDbInterface(db_name, netns, isTcpConn);
    if (nullptr == client)
    {
        return 1;
    }

    RedisReply reply(client.get(), operation);
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
        std::async(std::launch::async, handleSingleOperation, netns, db_name, operation, isTcpConn);
    }

    return 0;
}

int handleOperation(Options &options)
{
    if (!options.m_db_or_op.empty())
    {
        auto dbOrOperation = options.m_db_or_op;
        auto netns = options.m_namespace;
        bool isTcpConn = !options.m_unixsocket;
        if (netns != "None")
        {
            SonicDBConfig::initializeGlobalConfig(netns);
        }
        else
        {
            // Use the tcp connectivity if namespace is local and unixsocket cmd_option is present.
            isTcpConn = true;
        }
        
        if (options.m_cmd.size() != 0)
        {
            auto commands = options.m_cmd;
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
            printUsage();
        }
    }
    else
    {
        printUsage();
    }

    return 0;
}

void parseCliArguments(
    int argc,
    char** argv,
    Options &options)
{
    // Parse argument with getopt https://man7.org/linux/man-pages/man3/getopt.3.html
    const char* short_options = "hsn";
    static struct option long_options[] = {
       {"help",        optional_argument, NULL,  'h' },
       {"unixsocket",  optional_argument, NULL,  's' },
       {"namespace",   optional_argument, NULL,  'n' },
       // The last element of the array has to be filled with zeros.
       {0,          0,       0,  0 }
    };
    
    // prevent getopt_long print "invalid option" message.
    opterr = 0;

    while(optind < argc)
    {
        int opt = getopt_long(argc, argv, short_options, long_options, NULL);
        if (opt != -1)
        {
            switch (opt) {
                case 'h':
                    options.m_help = true;
                    break;

                case 's':
                    options.m_unixsocket = true;
                    break;

                case 'n':
                    if (optind < argc)
                    {
                        options.m_namespace = argv[optind];
                        optind++;
                    }
                    else
                    {
                        throw invalid_argument("namespace value is missing.");
                    }
                    break;

                default:
                   // argv contains unknown argument
                   throw invalid_argument("Unknown argument:" + string(argv[optind]));
            }
        }
        else
        {
            // db_or_op and cmd are non-option arguments
            // db_or_op argument
            options.m_db_or_op = argv[optind];
            optind++;

            // cmd arguments
            while(optind < argc)
            {
                auto cmdstr = string(argv[optind]);
                options.m_cmd.push_back(cmdstr);
                optind++;
            }
        }
    }
}

int sonic_db_cli(int argc, char** argv)
{
    Options options;
    try
    {
        parseCliArguments(argc, argv, options);
    }
    catch (invalid_argument const& e)
    {
        cerr << "Command Line Error: " << e.what() << endl;
        printUsage();
        return -1;
    }
    catch (logic_error const& e)
    {
        // getopt_long throw logic_error when found a unknown option without value.
        cerr << "Unknown option without value: " << endl;
        printUsage();
        return -1;
    }

    if (options.m_help)
    {
        printUsage();
        return 0;
    }

    try
    {
        return handleOperation(options);
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