#pragma once

#include <string>
#include <vector>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

void printUsage(const po::options_description &all_options);

void printRedisReply(redisReply* reply);

int executeCommands(
    const std::string& db_name,
    std::vector<std::string>& commands,
    const std::string& name_space,
    bool use_unix_socket);

void handleSingleOperation(
    const std::string& name_space,
    const std::string& db_name,
    const std::string& operation,
    bool use_unix_socket);

void handleAllInstances(
    const std::string& name_space,
    const std::string& operation,
    bool use_unix_socket);

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