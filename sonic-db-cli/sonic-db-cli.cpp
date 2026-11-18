#include <future>
#include <iostream>
#include <getopt.h>
#include <list>
#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/find.hpp>
#include "common/redisreply.h"
#include "common/redissocket.h"
#include "sonic-db-cli.h"

using namespace swss;
using namespace std;

namespace
{
struct File
{
    int fd;

    ~File()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
};

void exportRdb(const Options& options, bool useUnixSocket)
{
    const unsigned int timeout_ms = 60000;
    const auto& destination = options.m_rdb;
    const auto& db = options.m_db_or_op;
    const auto& netns = options.m_namespace;
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    File connection{-1};
    if (useUnixSocket)
    {
        connection.fd = openRedisUnixSocket(
            SonicDBConfig::getDbSock(db, netns), timeout_ms, options.m_authenticate);
    }
    else
    {
        if (options.m_authenticate)
        {
            throw std::invalid_argument("Authentication requires an explicitly selected local Unix socket");
        }
        const auto host = SonicDBConfig::getDbHostname(db, netns);
        const auto port = SonicDBConfig::getDbPort(db, netns);
        const timeval timeout{static_cast<time_t>(timeout_ms / 1000), 0};
        std::unique_ptr<redisContext, decltype(&redisFree)> context(
            redisConnectWithTimeout(host.c_str(), port, timeout), redisFree);
        if (!context)
        {
            throw std::bad_alloc();
        }
        if (context->err)
        {
            throw RedisError("RDB connection failed", context.get());
        }
        connection.fd = fcntl(context->fd, F_DUPFD_CLOEXEC, 0);
        if (connection.fd < 0)
        {
            throw std::system_error(errno, std::generic_category(), "RDB descriptor duplication");
        }
    }
    const int flags = fcntl(connection.fd, F_GETFL);
    if (flags < 0 || fcntl(connection.fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        throw std::system_error(errno, std::generic_category(), "RDB socket mode");
    }
    const auto wait = [&](short events)
    {
        for (;;)
        {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - Clock::now()).count();
            if (left < 0)
            {
                throw std::system_error(std::make_error_code(std::errc::timed_out));
            }
            const int timeout = static_cast<int>(std::min<long long>(left + 1, INT_MAX));
            pollfd event{connection.fd, events, 0};
            const int rc = poll(&event, 1, timeout);
            if (rc < 0 && errno == EINTR)
            {
                continue;
            }
            if (rc < 0)
            {
                throw std::system_error(errno, std::generic_category(), "RDB poll");
            }
            if (rc)
            {
                return;
            }
        }
    };
    const auto receive = [&](char *buffer, size_t size)
    {
        for (;;)
        {
            wait(POLLIN);
            const auto count = read(connection.fd, buffer, size);
            if (count < 0 && (errno == EINTR || errno == EAGAIN))
            {
                continue;
            }
            if (count <= 0)
            {
                throw std::runtime_error("RDB transfer truncated");
            }
            return static_cast<size_t>(count);
        }
    };
    const std::string command = "SYNC\r\n";
    size_t sent = 0;
    while (sent < command.size())
    {
        wait(POLLOUT);
        const auto count = send(
            connection.fd, command.data() + sent, command.size() - sent, MSG_NOSIGNAL);
        if (count < 0 && (errno == EINTR || errno == EAGAIN))
        {
            continue;
        }
        if (count <= 0)
        {
            throw std::runtime_error("Cannot request RDB snapshot");
        }
        sent += static_cast<size_t>(count);
    }
    std::string header;
    do
    {
        header.clear();
        for (;;)
        {
            char byte;
            receive(&byte, 1);
            if (byte == '\n')
            {
                break;
            }
            if (header.size() >= 128)
            {
                throw std::runtime_error("Invalid RDB framing");
            }
            header += byte;
        }
    }
    while (header.empty() || header == "\r");
    if (header.size() < 3 || header.front() != '$' || header.back() != '\r')
    {
        throw std::runtime_error("RDB export requires length framing");
    }
    const auto digits = header.substr(1, header.size() - 2);
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
    {
        throw std::runtime_error("RDB export requires length framing");
    }
    auto remaining = std::stoull(digits);
    if (!remaining)
    {
        throw std::runtime_error("Empty RDB snapshot");
    }
    std::vector<char> temporary(destination.begin(), destination.end());
    const char suffix[] = ".XXXXXX";
    temporary.insert(temporary.end(), suffix, suffix + sizeof(suffix));
    File output{mkostemp(temporary.data(), O_CLOEXEC)};
    if (output.fd < 0)
    {
        throw std::system_error(errno, std::generic_category(), "RDB output");
    }
    try
    {
        char buffer[65536];
        while (remaining)
        {
            const auto count = receive(buffer, static_cast<size_t>(
                std::min<unsigned long long>(remaining, sizeof(buffer))));
            size_t written = 0;
            while (written < count)
            {
                const auto size = write(output.fd, buffer + written, count - written);
                if (size < 0 && errno == EINTR)
                {
                    continue;
                }
                if (size <= 0)
                {
                    throw std::runtime_error("Cannot write RDB snapshot");
                }
                written += static_cast<size_t>(size);
            }
            remaining -= count;
        }
        if (fsync(output.fd) || rename(temporary.data(), destination.c_str()))
        {
            throw std::system_error(errno, std::generic_category(), "RDB replacement");
        }
    }
    catch (...)
    {
        unlink(temporary.data());
        throw;
    }
}
}

void printUsage()
{
    cout << "usage: sonic-db-cli [-h] [-s] [-j] [-n NAMESPACE] [--rdb FILE] db_or_op [cmd [cmd ...]]" << endl;
    cout << endl;
    cout << "SONiC DB CLI:" << endl;
    cout << endl;
    cout << "positional arguments:" << endl;
    cout << "  db_or_op              Database name Or Unary operation(only PING/SAVE/FLUSHALL supported)" << endl;
    cout << "  cmd                   Command to execute in database" << endl;
    cout << endl;
    cout << "optional arguments:" << endl;
    cout << "  -h, --help            show this help message and exit" << endl;
    cout << "  -s, --unixsocket      Override use of tcp_port and use unixsocket" << endl;
    cout << "      --authenticate    Request native local writer authentication (requires Unix)" << endl;
    cout << "      --rdb FILE        Save all databases in the selected Redis instance to FILE" << endl;
    cout << "  -j, --json            Print command result as JSON" << endl;
    cout << "  -n NAMESPACE, --namespace NAMESPACE" << endl;
    cout << "                        Namespace string to use asic0/asic1.../asicn" << endl;
    cout << endl;
    cout << "**sudo** needed for commands accesing a different namespace [-n], or using unixsocket connection [-s]" << endl;
    cout << endl;
    cout << "Example 1: sonic-db-cli -n asic0 CONFIG_DB keys \\*" << endl;
    cout << "Example 2: sonic-db-cli -n asic2 APPL_DB HGETALL VLAN_TABLE:Vlan10" << endl;
    cout << "Example 3: sonic-db-cli APPL_DB HGET VLAN_TABLE:Vlan10 mtu" << endl;
    cout << "Example 4: sonic-db-cli -n asic3 APPL_DB EVAL \"return {KEYS[1],KEYS[2],ARGV[1],ARGV[2]}\" 2 k1 k2 v1 v2" << endl;
    cout << "Example 5: sonic-db-cli PING | sonic-db-cli -s PING" << endl;
    cout << "Example 6: sonic-db-cli SAVE | sonic-db-cli -s SAVE" << endl;
    cout << "Example 7: sonic-db-cli FLUSHALL | sonic-db-cli -s FLUSHALL" << endl;
    cout << "Example 8: sonic-db-cli -s --authenticate --rdb /tmp/redis.rdb ASIC_DB" << endl;
}

string handleSingleOperation(
    const string& netns,
    const string& db_name,
    const string& operation,
    bool useUnixSocket,
    bool authenticate)
{
    shared_ptr<DBConnector> client;
    auto host = SonicDBConfig::getDbHostname(db_name, netns);
    string message = "Could not connect to Redis at " + host + ":";
    try
    {
        auto db_id =  SonicDBConfig::getDbId(db_name, netns);
        if (authenticate)
        {
            if (!useUnixSocket || host == "redis_chassis.server")
                throw std::invalid_argument("Authentication requires an explicitly selected local Unix socket");
            client = make_shared<DBConnector>(db_name, 0, false, netns, true);
        }
        else if (useUnixSocket && host != "redis_chassis.server")
        {
            auto db_socket = SonicDBConfig::getDbSock(db_name, netns);
            message += db_name + ": Connection refused";
            client = make_shared<DBConnector>(db_id, db_socket, 0);
        }
        else
        {
            auto port = SonicDBConfig::getDbPort(db_name, netns);
            message += std::to_string(port) + ": Connection refused";
            client = make_shared<DBConnector>(db_id, host, port, 0);
        }
    }
    catch (const exception& e)
    {
        if (authenticate)
            message += db_name + ": " + e.what();
        return message;
    }

    if (operation == "PING"
        || operation == "SAVE"
        || operation == "FLUSHALL")
    {
        RedisReply reply(client.get(), operation);
        auto response = reply.getContext();
        if (nullptr != response)
        {
            return string();
        }
    }
    else
    {
        throw std::invalid_argument("Operation " + operation +" is not supported");
    }

    return message;
}

int handleAllInstances(
    const string& netns,
    const string& operation,
    bool useUnixSocket,
    bool authenticate)
{
    auto db_names = SonicDBConfig::getDbList(netns);
    // Operate All Redis Instances in Parallel
    // TODO: if one of the operations failed, it could fail quickly and not necessary to wait all other operations
    list<future<string>> responses;
    for (auto& db_name : db_names)
    {
        future<string> response = std::async(std::launch::async, handleSingleOperation, netns, db_name, operation, useUnixSocket, authenticate);
        responses.push_back(std::move(response));
    }

    bool operation_failed = false;
    for (auto& response : responses)
    {
        auto respstr = response.get();
        if (respstr != "")
        {
            cout << respstr << endl;
            operation_failed = true;
        }
    }

    if (operation_failed)
    {
        return 1;
    }
    
    if (operation == "PING")
    {
        cout << "PONG" << endl;
    }
    else
    {
        cout << "OK" << endl;
    }

    return 0;
}

int executeCommands(
    const string& db_name,
    vector<string>& commands,
    const string& netns,
    bool useUnixSocket,
    bool useJson,
    bool authenticate)
{
    shared_ptr<DBConnector> client = nullptr;
    try
    {
        int db_id =  SonicDBConfig::getDbId(db_name, netns);
        auto host = SonicDBConfig::getDbHostname(db_name, netns);
        if (authenticate)
        {
            if (!useUnixSocket || host == "redis_chassis.server")
                throw std::invalid_argument("Authentication requires an explicitly selected local Unix socket");
            client = make_shared<DBConnector>(db_name, 0, false, netns, true);
        }
        else if (useUnixSocket && host != "redis_chassis.server")
        {
            auto db_socket = SonicDBConfig::getDbSock(db_name, netns);
            client = make_shared<DBConnector>(db_id, db_socket, 0);
        }
        else
        {
            auto port = SonicDBConfig::getDbPort(db_name, netns);
            client = make_shared<DBConnector>(db_id, host, port, 0);
        }
    }
    catch (const exception& e)
    {
        cerr << "Invalid database name input : '" << db_name << "'" << endl;
        cerr << e.what() << endl;
        return 1;
    }

    try
    {
        RedisCommand command;
        command.format(commands);
        RedisReply reply(client.get(), command);
        /*
        sonic-db-cli output format mimic the non-tty mode output format from redis-cli
        based on our usage in SONiC, None and list type output from python API needs to be modified
        with these changes, it is enough for us to mimic redis-cli in SONiC so far since no application uses tty mode redis-cli output
        */
        auto commandName = getCommandName(commands);
        if (useJson)
        {
            cout << RedisReply::to_json_string(reply.getContext(), commandName) << endl;
        }
        else
        {
            cout << RedisReply::to_string(reply.getContext(), commandName) << endl;
        }
    }
    catch (const std::system_error& e)
    {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}

void parseCliArguments(
    int argc,
    char** argv,
    Options &options)
{
    // Parse argument with getopt https://man7.org/linux/man-pages/man3/getopt.3.html
    const char* short_options = "hsjn";
    static struct option long_options[] = {
       {"help",        optional_argument, NULL,  'h' },
       {"unixsocket",  optional_argument, NULL,  's' },
       {"authenticate", no_argument, NULL, 'a' },
       {"rdb",         required_argument, NULL, 'r' },
       {"json",        no_argument,       NULL,  'j' },
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

                case 'a':
                    options.m_authenticate = true;
                    break;

                case 'j':
                    options.m_json = true;
                    break;

                case 'r':
                    options.m_rdb = optarg;
                    if (options.m_rdb.empty())
                    {
                        throw invalid_argument("--rdb requires a destination filename");
                    }
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
                   if (optopt == 'r')
                   {
                       throw invalid_argument("--rdb requires a destination filename");
                   }
                   // argv contains unknown argument
                   throw invalid_argument("Unknown argument:" + string(argv[optind]));
            }
        }
        else
        {
            // db_or_op and cmd are non-option arguments
            options.m_db_or_op = argv[optind];
            optind++;

            while(optind < argc)
            {
                auto cmdstr = string(argv[optind]);
                options.m_cmd.push_back(cmdstr);
                optind++;
            }
        }
    }
}

int sonic_db_cli(
    int argc,
    char** argv,
    function<void()> initializeGlobalConfig,
    function<void()> initializeConfig)
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
        cerr << "Unknown option without value: "  << e.what() << endl;
        printUsage();
        return -1;
    }

    if (options.m_help)
    {
        printUsage();
        return 0;
    }

    if (!options.m_rdb.empty() &&
        (options.m_db_or_op.empty() || !options.m_cmd.empty() || options.m_json))
    {
        cerr << "--rdb requires a database name and cannot be combined with a command or --json" << endl;
        return 1;
    }

    if (!options.m_db_or_op.empty())
    {
        auto dbOrOperation = options.m_db_or_op;
        auto netns = options.m_namespace;
        bool useUnixSocket = options.m_unixsocket;
        // Load the database config for the namespace
        if (!netns.empty())
        {
            initializeGlobalConfig();

            // Use the unix domain connectivity if namespace not empty.
            useUnixSocket = true;
        }

        if (!options.m_rdb.empty())
        {
            if (netns.empty())
            {
                initializeConfig();
            }
            exportRdb(options, useUnixSocket);
            return 0;
        }
        else if (options.m_cmd.size() != 0)
        {
            auto commands = options.m_cmd;

            if (netns.empty())
            {
                initializeConfig();
            }

            return executeCommands(dbOrOperation, commands, netns, useUnixSocket, options.m_json, options.m_authenticate);
        }
        else if (dbOrOperation == "PING"
                || dbOrOperation == "SAVE"
                || dbOrOperation == "FLUSHALL")
        {
            // redis-cli doesn't depend on database_config.json which could raise some exceptions
            // sonic-db-cli catch all possible exceptions and handle it as a failure case which not return 'OK' or 'PONG'
            try
            {
                if (netns.empty())
                {
                    // When database_config.json does not exist, sonic-db-cli will ignore exception and return 1.
                    initializeConfig();
                }

                return handleAllInstances(netns, dbOrOperation, useUnixSocket, options.m_authenticate);
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


int cli_exception_wrapper(
    int argc,
    char** argv,
    function<void()> initializeGlobalConfig,
    function<void()> initializeConfig)
{
    try
    {
        return sonic_db_cli(
                        argc,
                        argv,
                        initializeGlobalConfig,
                        initializeConfig);
    }
    catch (const exception& e)
    {
        // sonic-db-cli is porting from python version.
        // in python version, when any exception happen sonic-db-cli will crash but will not generate core dump file.
        // catch all exception here to avoid a core dump file generated.
        cerr << "An exception of type " << e.what() << " occurred. Arguments:" << endl;
        for (int idx = 0; idx < argc; idx++)
        {
            cerr << argv[idx]  << " ";
        }
        cerr << endl;

        // when python version crash, exit code is 1.
        return 1;
    }
}

string getCommandName(vector<string>& commands)
{
    if (commands.size() == 0)
    {
        return string();
    }

    return boost::to_upper_copy<string>(commands[0]);
}
