#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/json.hpp"
#include <unordered_map>

using namespace std;
using namespace swss;
using json = nlohmann::json;

extern string global_existing_file;

TEST(DBConnector, multi_ns_test)
{
    std::string local_file, dir_name, ns_name;
    vector<string> namespaces;
    vector<string> ns_names;

    // load global config file again, should throw exception with init already done
    try
    {
        cout<<"INIT: loading local config file again"<<endl;
        SonicDBConfig::initializeGlobalConfig(global_existing_file);
    }
    catch (exception &e)
    {
        EXPECT_TRUE(strstr(e.what(), "SonicDBConfig already initialized"));
    }

    ifstream f(global_existing_file);
    if (f.good())
    {
        json g;
        f >> g;

        // Get the directory name from the file path given as input.
        std::string::size_type pos = global_existing_file.rfind("/");
        dir_name = global_existing_file.substr(0,pos+1);

        for (auto& element : g["INCLUDES"])
        {
            local_file.append(dir_name);
            local_file.append(element["include"]);

            if(element["namespace"].empty())
            {
                ns_name = EMPTY_NAMESPACE;
            }
            else
            {
                ns_name = element["namespace"];
                namespaces.push_back(ns_name);
            }

            // parse config file
            ifstream i(local_file);

            if (i.good())
            {
                json j;
                i >> j;
                unordered_map<string, RedisInstInfo> m_inst_info;
                for (auto it = j["INSTANCES"].begin(); it!= j["INSTANCES"].end(); it++)
                {
                   string instName = it.key();
                   string socket = it.value().at("unix_socket_path");
                   string hostname = it.value().at("hostname");
                   int port = it.value().at("port");
                   m_inst_info[instName] = {socket, hostname, port};
                }

                for (auto it = j["DATABASES"].begin(); it!= j["DATABASES"].end(); it++)
                {
                   string dbName = it.key();
                   string instName = it.value().at("instance");
                   int dbId = it.value().at("id");
                   cout<<"testing "<<dbName<<endl;
                   cout<<instName<<"#"<<dbId<<"#"<<m_inst_info[instName].unixSocketPath<<"#"<<m_inst_info[instName].hostname<<"#"<<m_inst_info[instName].port<<endl;
                   // dbInst info matches between get api and context in json file
                   EXPECT_EQ(instName, SonicDBConfig::getDbInst(dbName, ns_name));
                   // dbId info matches between get api and context in json file
                   EXPECT_EQ(dbId, SonicDBConfig::getDbId(dbName, ns_name));
                   // socket info matches between get api and context in json file
                   EXPECT_EQ(m_inst_info[instName].unixSocketPath, SonicDBConfig::getDbSock(dbName, ns_name));
                   // hostname info matches between get api and context in json file
                   EXPECT_EQ(m_inst_info[instName].hostname, SonicDBConfig::getDbHostname(dbName, ns_name));
                   // port info matches between get api and context in json file
                   EXPECT_EQ(m_inst_info[instName].port, SonicDBConfig::getDbPort(dbName, ns_name));
                }
            }
             local_file.clear();
        }
    }

    // Get the namespaces from the database_global.json file and compare with the list we created here.
    ns_names = SonicDBConfig::getNamespaces();
    sort (namespaces.begin(), namespaces.end());
    sort (ns_names.begin(), ns_names.end());
    EXPECT_EQ(ns_names.size(), namespaces.size());
    EXPECT_TRUE(namespaces == ns_names);
}
