#include <iostream>
#include <iomanip>
#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
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
              << "\t -c\tcomponent name in DB for which the setting is applied" << std::endl
              << "\t -a\tapply the setting to all components" << std::endl
              << "\t -s\tapply loglevel for SAI api component (equivalent to adding prefix \"SAI_API_\" to component)" << std::endl
              << "\t -p\tprint components registered or configured in DB for which setting can be applied" << std::endl
              << "\t -d\tset every configured component to the default loglevel, keeping its row" << std::endl
              << "\t -r\tdelete the configured row of a component (provided with -c or -a), returning it to its own default" << std::endl<< std::endl
              << "Examples:" << std::endl
              << "\t" << program << " -l NOTICE -c orchagent # set orchagent severity level to NOTICE" << std::endl
              << "\t" << program << " -l SAI_LOG_LEVEL_ERROR -s -c SWITCH # set SAI_API_SWITCH severity to ERROR" << std::endl
              << "\t" << program << " -l DEBUG -a # set all not SAI components severity to DEBUG" << std::endl
              << "\t" << program << " -l SAI_LOG_LEVEL_DEBUG -s -a # set all SAI_API_* severity to DEBUG" << std::endl
              << "\t" << program << " -d # set every configured component to the default loglevel" << std::endl
              << "\t" << program << " -r -c orchagent # delete the orchagent row, returning it to its default" << std::endl;

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
    bool applyToAll = false, print = false, default_loglevel_opt = false, removeSetting = false;
    std::string prefix = "", component, loglevel;
    auto exitWithUsage = std::bind(usage, argv[0], std::placeholders::_1, std::placeholders::_2);

    while ( (opt = getopt (argc, argv, "c:l:sapdrh")) != -1)
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
            case 'r':
                removeSetting = true;
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
    std::vector<std::string> configured;
    logger_tbl.getKeys(configured);

    // The components are the union of the CONFIG_DB rows and the STATE_DB
    // registrations; either can name a component the other lacks.
    std::vector<std::string> keys = configured;
    std::unique_ptr<DBConnector> state_db;
    std::unique_ptr<swss::Table> state_logger_tbl;
    try
    {
        state_db = std::make_unique<DBConnector>("STATE_DB", 0);
        state_logger_tbl = std::make_unique<swss::Table>(state_db.get(), STATE_LOGGER_TABLE_NAME);
        std::vector<std::string> registered;
        state_logger_tbl->getKeys(registered);
        keys.insert(keys.end(), registered.begin(), registered.end());
    }
    catch (const std::exception& e)
    {
        std::cerr << "STATE_DB unavailable, listing configured components only: " << e.what() << std::endl;
        state_logger_tbl.reset();
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    // A STATE_DB read that fails later degrades the same way as one that
    // fails above: the rest of the command runs without STATE_DB.
    auto stateHget = [&state_logger_tbl](const std::string& key, const std::string& field, std::string& value) {
        if (!state_logger_tbl)
        {
            return false;
        }
        try
        {
            return state_logger_tbl->hget(key, field, value);
        }
        catch (const std::exception& e)
        {
            std::cerr << "STATE_DB unavailable, continuing without it: " << e.what() << std::endl;
            state_logger_tbl.reset();
            return false;
        }
    };

    // A component that reads its level only on SIGHUP marks its STATE_DB row.
    auto printRefreshHints = [&stateHget](const std::vector<std::string>& components) {
        for (const auto& key : components)
        {
            std::string refresh;
            if (stateHget(key, "require_manual_refresh", refresh) && refresh == "true")
            {
                std::cout << key << " applies the change once it receives SIGHUP" << std::endl;
            }
        }
    };

    if (print)
    {
        int errorCount = 0;

        if (argc != 2)
        {
            exitWithUsage(EXIT_FAILURE, "-p option does not accept other options");
        }

        for (const auto& key : keys)
        {
            std::string level;
            if (!logger_tbl.hget(key, DAEMON_LOGLEVEL, level) && !stateHget(key, DAEMON_LOGLEVEL, level))
            {
                if (!state_logger_tbl && std::find(configured.begin(), configured.end(), key) == configured.end())
                {
                    // Listed by STATE_DB, which is now unreachable.
                    continue;
                }
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

    if (default_loglevel_opt && removeSetting)
    {
        exitWithUsage(EXIT_FAILURE, "-d and -r cannot be used together");
    }

    if(default_loglevel_opt)
    {
        // Only configured rows are set to NOTICE (SAI_LOG_LEVEL_NOTICE for SAI
        // keys); no row is created. A component without a row keeps its own
        // default, which may differ from NOTICE.
        std::vector<std::string> sai_keys = get_sai_keys(configured);
        std::vector<std::string> no_sai_keys = get_no_sai_keys(configured);
        setAllLoglevel(logger_tbl,no_sai_keys, std::string(DEFAULT_LOGLEVEL));
        setAllLoglevel(logger_tbl,sai_keys, std::string(SAI_DEFAULT_LOGLEVEL));
        printRefreshHints(configured);
        return (EXIT_SUCCESS);
    }
    if (removeSetting)
    {
        // The whole row is deleted, so the setting thread applies the
        // component's own default; deleting only LOGLEVEL would leave a row
        // behind that -p keeps listing.
        if (loglevel != "" || (applyToAll == (component != "")))
        {
            exitWithUsage(EXIT_FAILURE, "-r takes either -c or -a, and no -l");
        }

        std::vector<std::string> targets;
        if (applyToAll)
        {
            targets = prefix == "SAI_API_" ? get_sai_keys(configured) : get_no_sai_keys(configured);
        }
        else
        {
            component = prefix + component;
            if (std::find(std::begin(keys), std::end(keys), component) == keys.end())
            {
                exitWithUsage(EXIT_FAILURE, "Component not present in DB");
            }
            targets.push_back(component);
        }

        std::vector<std::string> removed;
        for (const auto& key : targets)
        {
            if (std::find(configured.begin(), configured.end(), key) == configured.end())
            {
                continue;
            }
            logger_tbl.del(key);
            removed.push_back(key);
        }
        printRefreshHints(removed);
        return EXIT_SUCCESS;
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
        printRefreshHints(keys);
        exit(EXIT_SUCCESS);
    }

    component = prefix + component;
    if (std::find(std::begin(keys), std::end(keys), component) == keys.end())
    {
        exitWithUsage(EXIT_FAILURE, "Component not present in DB");
    }
    setLoglevel(logger_tbl, component, loglevel);
    printRefreshHints({ component });

    return EXIT_SUCCESS;
}
