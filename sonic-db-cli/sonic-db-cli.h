#pragma once

#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include "common/dbconnector.h"
#include "common/dbinterface.h"
#include "common/redisreply.h"

namespace po = boost::program_options;

void printUsage(const po::options_description &all_options);

void printRedisReply(swss::RedisReply& reply);

std::string buildRedisOperation(std::vector<std::string>& commands);

int connectDbInterface(
    swss::DBInterface& dbintf,
    const std::string& db_name,
    const std::string& netns,
    bool isTcpConn);

int executeCommands(
    const std::string& db_name,
    std::vector<std::string>& commands,
    const std::string& netns,
    bool isTcpConn);

int handleSingleOperation(
    const std::string& netns,
    const std::string& db_name,
    const std::string& operation,
    bool isTcpConn);

int handleAllInstances(
    const std::string& netns,
    const std::string& operation,
    bool isTcpConn);

int handleOperation(
    const po::options_description &all_options,
    const po::variables_map &variables_map);

void parseCliArguments(
    int argc,
    char** argv,
    po::options_description &all_options,
    po::variables_map &variables_map);

#ifdef TESTING
int sonic_db_cli(int argc, char** argv);
#endif