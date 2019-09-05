#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/json.hpp"
#include <unordered_map>

using namespace std;
using namespace swss;
using json = nlohmann::json;


TEST(DBConnector, multi_db_test)
{

    string file = "./tests/redis_multi_db_ut_config/database_config.json";
    // default config file should not be found
    cout<<"Default : load init db config file, isInit = "<<RedisConfig::isInit()<<endl;
    EXPECT_EQ(RedisConfig::isInit(), false);
    // update with local config file, should be found
    RedisConfig::updateDBConfigFile(file);
    cout<<"Update : load local db config file, isInit = "<<RedisConfig::isInit()<<endl;
    EXPECT_EQ(RedisConfig::isInit(), true);

    // parse config file
    ifstream i(file);
    if (i.good()) {
        json j;
        i >> j;
        unordered_map<string, pair<string, int>> m_redis_info;
        for(auto it = j["INSTANCES"].begin(); it!= j["INSTANCES"].end(); it++) {
           string instName = it.key();
           string sockPath = it.value().at("socket");
           int port = it.value().at("port");
           m_redis_info[instName] = {sockPath, port};
        }

        for(auto it = j["DATABASES"].begin(); it!= j["DATABASES"].end(); it++) {
           string instName = it.value().at("instance");
           int dbId = it.value().at("id");
           cout<<"JSON file # dbid: "<<dbId<<" ,socket: "<<m_redis_info[instName].first<<" .port: "<<m_redis_info[instName].second<<endl;
           cout<<"API get   # dbid: "<<dbId<<" ,socket: "<<RedisConfig::getDbsock(dbId)<<" .port: "<<RedisConfig::getDbport(dbId)<<endl;
           // socket info matches between get api and context in json file
           EXPECT_EQ(m_redis_info[instName].first, RedisConfig::getDbsock(dbId));
           // port info matches between get api and context in json file
           EXPECT_EQ(m_redis_info[instName].second, RedisConfig::getDbport(dbId));
        }
    }
}
