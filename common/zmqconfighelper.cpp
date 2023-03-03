#include <string>
#include <fstream>
#include <system_error>
#include <cerrno>
#include <cstring>

#include "zmqconfighelper.h"
#include "json.hpp"
#include "logger.h"

using namespace std;
using namespace nlohmann;

namespace swss {

int ZmqConfigHelper::GetSocketPort(string db, string tableName, std::string configPath)
{
    ifstream ifs(configPath);
    if (!ifs.good())
    {
        SWSS_LOG_ERROR("failed to read '%s', err: %s", configPath.c_str(), strerror(errno));
        throw system_error(make_error_code(errc::no_such_file_or_directory), "Failed to read config file");
    }

    try
    {
        json config;
        ifs >> config;
        json& item = config[db][tableName];
        return item[ZMQ_PORT_CONFIG_NAME];
    }
    catch (const std::exception& e)
    {
        SWSS_LOG_ERROR("Failed to find ZMQ socket port for database: %s, table: %s, error: %s", db.c_str(), tableName.c_str(), e.what());
        throw e;
    }
}

}
