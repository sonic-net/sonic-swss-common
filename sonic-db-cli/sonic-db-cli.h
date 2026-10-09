#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "common/dbconnector.h"
#include "common/dbinterface.h"
#include "common/redisreply.h"

enum class CliEndpointType
{
    AUTO,
    UNIX_SOCKET,
    TCP
};

struct Options
{
    bool m_help = false;
    bool m_json = false;
    CliEndpointType m_endpointType = CliEndpointType::AUTO;
    std::string m_namespace;
    std::string m_db_or_op;
    std::vector<std::string> m_cmd;
};

void printUsage();

void printRedisReply(swss::RedisReply& reply);

std::shared_ptr<swss::DBConnector> connectToDatabase(
    const std::string& db_name,
    const std::string& netns,
    CliEndpointType endpointType);

int executeCommands(
    const std::string& db_name,
    std::vector<std::string>& commands,
    const std::string& netns,
    CliEndpointType endpointType,
    bool useJson = false);

std::string handleSingleOperation(
    const std::string& netns,
    const std::string& db_name,
    const std::string& operation,
    CliEndpointType endpointType);

int handleAllInstances(
    const std::string& netns,
    const std::string& operation,
    CliEndpointType endpointType);

void parseCliArguments(
    int argc,
    char** argv,
    Options &options);

int sonic_db_cli(
    int argc,
    char** argv,
    std::function<void()> initializeGlobalConfig,
    std::function<void()> initializeConfig);

int cli_exception_wrapper(
    int argc,
    char** argv,
    std::function<void()> initializeGlobalConfig,
    std::function<void()> initializeConfig);

std::string getCommandName(std::vector<std::string>& command);
