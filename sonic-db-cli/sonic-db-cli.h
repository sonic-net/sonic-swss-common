#pragma once

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "common/dbconnector.h"
#include "common/dbinterface.h"
#include "common/redisreply.h"

struct Options
{
    bool m_help = false;
    bool m_unixsocket = false;
    std::string m_namespace;
    std::string m_db_or_op;
    std::vector<std::string> m_cmd;
};

void printUsage();

void printRedisReply(swss::RedisReply& reply);

std::shared_ptr<swss::DBConnector> connectDbInterface(
    const std::string& db_name,
    const std::string& netns,
    bool isTcpConn);

int executeCommands(
    const std::string& db_name,
    std::vector<std::string>& commands,
    const std::string& netns,
    bool isTcpConn);

std::string handleSingleOperation(
    const std::string& netns,
    const std::string& db_name,
    const std::string& operation,
    bool isTcpConn);

int handleAllInstances(
    const std::string& netns,
    const std::string& operation,
    bool isTcpConn);

void parseCliArguments(
    int argc,
    char** argv,
    Options &options);

int sonic_db_cli(
    int argc,
    char** argv,
    std::function<void()> initializeGlobalConfig,
    std::function<void()> initializeConfig);

std::string getCommandName(std::vector<std::string>& command);