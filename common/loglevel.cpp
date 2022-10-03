#include <iostream>
#include <iomanip>
#include <vector>
#include <functional>
#include <algorithm>
#include <unistd.h>
#include "schema.h"
#include "loglevel.h"
#include "logger.h"
#include "dbconnector.h"
#include "producerstatetable.h"

using namespace swss;

[[ noreturn ]] void usage(const std::string &program, int status, const std::string &message)
{
    if (message.size() != 0)
    {
        std::cout << message << std::endl << std::endl;
    }

    std::cout << "Usage: " << program << " [OPTIONS]" << std::endl
              << "SONiC logging severity level setting." << std::endl << std::endl
              << "Options:" << std::endl
              << "\t -h\tprint this message" << std::endl
              << "\t -l\tloglevel value" << std::endl
              << "\t -c\tcomponent name in DB for which loglevel is applied (provided with -l)" << std::endl
              << "\t -a\tapply loglevel to all components (provided with -l)" << std::endl
              << "\t -s\tapply loglevel for SAI api component (equivalent to adding prefix \"SAI_API_\" to component)" << std::endl
              << "\t -p\tprint components registered in DB for which setting can be applied" << std::endl
              << "\t -d\treturn all components to default loglevel" << std::endl<< std::endl
              << "Examples:" << std::endl
              << "\t" << program << " -l NOTICE -c orchagent # set orchagent severity level to NOTICE" << std::endl
              << "\t" << program << " -l SAI_LOG_LEVEL_ERROR -s -c SWITCH # set SAI_API_SWITCH severity to ERROR" << std::endl
              << "\t" << program << " -l DEBUG -a # set all not SAI components severity to DEBUG" << std::endl
              << "\t" << program << " -l SAI_LOG_LEVEL_DEBUG -s -a # set all SAI_API_* severity to DEBUG" << std::endl
              << "\t" << program << " -d # return all components to default loglevel" << std::endl;

    exit(status);
}

void setLoglevel(swss::Table& logger_tbl, const std::string& component, const std::string& loglevel)
{
    logger_tbl.hset(component, "LOGLEVEL",loglevel);
}

bool validateSaiLoglevel(const std::string &prioStr)
{
    static const std::vector<std::string> saiPrios = {
        "SAI_LOG_LEVEL_CRITICAL",
        "SAI_LOG_LEVEL_ERROR",
        "SAI_LOG_LEVEL_WARN",
        "SAI_LOG_LEVEL_NOTICE",
        "SAI_LOG_LEVEL_INFO",
        "SAI_LOG_LEVEL_DEBUG",
    };

    return std::find(saiPrios.begin(), saiPrios.end(), prioStr) != saiPrios.end();
}

bool filterOutSaiKeys(const std::string& key)
{
    return key.find("SAI_API_") != std::string::npos;
}

bool filterSaiKeys(const std::string& key)
{
    return key.find("SAI_API_") == std::string::npos;
}

std::vector<std::string> get_sai_keys(std::vector<std::string> keys)
{
    keys.erase(std::remove_if(keys.begin(), keys.end(), filterSaiKeys), keys.end());
    return keys;
}

std::vector<std::string> get_no_sai_keys(std::vector<std::string> keys)
{
    keys.erase(std::remove_if(keys.begin(), keys.end(), filterOutSaiKeys), keys.end());
    return keys;
}

void setAllLoglevel(swss::Table& logger_tbl, std::vector<std::string> components, std::string loglevel)
{
    for (const auto& component : components)
    {
        setLoglevel(logger_tbl, component, loglevel);
    }
    SWSS_LOG_DEBUG("All components are with %s loglevel", loglevel.c_str());
}

int swssloglevel(int argc, char** argv)
{
    int opt;
    bool applyToAll = false, print = false, default_loglevel_opt = false;
    std::string prefix = "", component, loglevel;
    auto exitWithUsage = std::bind(usage, argv[0], std::placeholders::_1, std::placeholders::_2);

    while ( (opt = getopt (argc, argv, "c:l:sapdh")) != -1)
    {
        switch(opt)
        {
            case 'c':
                component = optarg;
                break;
            case 'l':
                loglevel = optarg;
                break;
            case 's':
                prefix = "SAI_API_";
                break;
            case 'a':
                applyToAll = true;
                break;
            case 'p':
                print = true;
                break;
            case 'd':
                default_loglevel_opt = true;
                break;
            case 'h':
                exitWithUsage(EXIT_SUCCESS, "");
                break;
            default:
                exitWithUsage(EXIT_FAILURE, "Invalid option");
        }
    }

    DBConnector config_db("CONFIG_DB", 0);
    swss::Table logger_tbl(&config_db, CFG_LOGGER_TABLE_NAME);
    std::vector<std::string> keys;
    logger_tbl.getKeys(keys);

    if (print)
    {
        int errorCount = 0;

        if (argc != 2)
        {
            exitWithUsage(EXIT_FAILURE, "-p option does not accept other options");
        }

        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys)
        {
            std::string level;
            if (!(logger_tbl.hget(key, DAEMON_LOGLEVEL, level)))
            {
                std::cerr << std::left << std::setw(30) << key << "Unknown log level" << std::endl;
                errorCount ++;
            }
            else
            {
                std::cout << std::left << std::setw(30) << key << level << std::endl;
            }
        }

        if (errorCount > 0)
            return (EXIT_FAILURE);

        return (EXIT_SUCCESS);
    }

    if(default_loglevel_opt)
    {
        std::vector<std::string> sai_keys = get_sai_keys(keys);
        std::vector<std::string> no_sai_keys = get_no_sai_keys(keys);
        setAllLoglevel(logger_tbl,no_sai_keys, std::string(DEFAULT_LOGLEVEL));
        setAllLoglevel(logger_tbl,sai_keys, std::string(SAI_DEFAULT_LOGLEVEL));
        return (EXIT_SUCCESS);
    }
    if ((prefix == "SAI_API_") && !validateSaiLoglevel(loglevel))
    {
        exitWithUsage(EXIT_FAILURE, "Invalid SAI loglevel value");
    }
    else if ((prefix == "") && (Logger::priorityStringMap.find(loglevel) == Logger::priorityStringMap.end()))
    {
        exitWithUsage(EXIT_FAILURE, "Invalid loglevel value");
    }

    if (applyToAll)
    {
        if (component != "")
        {
            exitWithUsage(EXIT_FAILURE, "Invalid options provided with -a");
        }

        if (prefix == "SAI_API_")
        {
            keys.erase(std::remove_if(keys.begin(), keys.end(), filterSaiKeys), keys.end());
        }
        else
        {
            keys.erase(std::remove_if(keys.begin(), keys.end(), filterOutSaiKeys), keys.end());
        }

        setAllLoglevel(logger_tbl, keys, loglevel);
        exit(EXIT_SUCCESS);
    }

    component = prefix + component;
    if (std::find(std::begin(keys), std::end(keys), component) == keys.end())
    {
        exitWithUsage(EXIT_FAILURE, "Component not present in DB");
    }
    setLoglevel(logger_tbl, component, loglevel);

    return EXIT_SUCCESS;
}
