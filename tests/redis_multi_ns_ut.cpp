#include <iostream>
#include <fstream>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/json.hpp"
#include "common/table.h"
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

    //Invalid DB name input, with the namespace input.
    cout<<"GET: invalid dbname input for getDbInst()"<<endl;
    EXPECT_THROW(SonicDBConfig::getDbInst("INVALID_DBNAME", EMPTY_NAMESPACE), out_of_range);

    cout<<"GET: invalid dbname input for getDbId()"<<endl;
    EXPECT_THROW(SonicDBConfig::getDbId("INVALID_DBNAME", EMPTY_NAMESPACE), out_of_range);

    cout<<"GET: invalid dbname input for getDbSock()"<<endl;
    EXPECT_THROW(SonicDBConfig::getDbSock("INVALID_DBNAME", EMPTY_NAMESPACE), out_of_range);

    cout<<"GET: invalid dbname input for getDbHostname()"<<endl;
    EXPECT_THROW(SonicDBConfig::getDbHostname("INVALID_DBNAME", EMPTY_NAMESPACE), out_of_range);

    cout<<"GET: invalid dbname input for getDbPort()"<<endl;
    EXPECT_THROW(SonicDBConfig::getDbPort("INVALID_DBNAME", EMPTY_NAMESPACE), out_of_range);

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
            }
            namespaces.push_back(ns_name);

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
                   cout<<ns_name<<"#"<<dbId<<dbName<<"#"<<m_inst_info[instName].unixSocketPath<<"#"<<m_inst_info[instName].hostname<<"#"<<m_inst_info[instName].port<<endl;
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

void clearNetnsDB()
{
    DBConnector db("STATE_DB2", 0, true, "asic0");
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void TableNetnsTest(string tableName)
{

    DBConnector db("STATE_DB2", 0, true, "asic0");

    Table t(&db, tableName);

    clearNetnsDB();
    cout << "Starting table manipulations" << endl;

    string key_1 = "a";
    string key_2 = "b";
    vector<FieldValueTuple> values;

    for (int i = 1; i < 4; i++)
    {
        string field = "field_" + to_string(i);
        string value = to_string(i);
        values.push_back(make_pair(field, value));
    }

    cout << "- Step 1. SET" << endl;
    cout << "Set key [a] field_1:1 field_2:2 field_3:3" << endl;
    cout << "Set key [b] field_1:1 field_2:2 field_3:3" << endl;

    t.set(key_1, values);
    t.set(key_2, values);

    cout << "- Step 2. GET_TABLE_KEYS" << endl;
    vector<string> keys;
    t.getKeys(keys);
    EXPECT_EQ(keys.size(), (size_t)2);

    for (auto k : keys)
    {
        cout << "Get key [" << k << "]" << flush;
        EXPECT_EQ(k.length(), (size_t)1);
    }

    cout << "- Step 3. GET_TABLE_CONTENT" << endl;
    vector<KeyOpFieldsValuesTuple> tuples;
    t.getContent(tuples);

    cout << "Get total " << tuples.size() << " number of entries" << endl;
    EXPECT_EQ(tuples.size(), (size_t)2);

    for (auto tuple: tuples)
    {
        cout << "Get key [" << kfvKey(tuple) << "]" << flush;
        unsigned int size_v = 3;
        EXPECT_EQ(kfvFieldsValues(tuple).size(), size_v);
        for (auto fv: kfvFieldsValues(tuple))
        {
            string value_1 = "1", value_2 = "2";
            cout << " " << fvField(fv) << ":" << fvValue(fv) << flush;
            if (fvField(fv) == "field_1")
            {
                EXPECT_EQ(fvValue(fv), value_1);
            }
            if (fvField(fv) == "field_2")
            {
                EXPECT_EQ(fvValue(fv), value_2);
            }
        }
        cout << endl;
    }

    cout << "- Step 4. DEL" << endl;
    cout << "Delete key [a]" << endl;
    t.del(key_1);

    cout << "- Step 5. GET" << endl;
    cout << "Get key [a] and key [b]" << endl;
    EXPECT_EQ(t.get(key_1, values), false);
    t.get(key_2, values);

    cout << "Get key [b]" << flush;
    for (auto fv: values)
    {
        string value_1 = "1", value_2 = "2";
        cout << " " << fvField(fv) << ":" << fvValue(fv) << flush;
        if (fvField(fv) == "field_1")
        {
            EXPECT_EQ(fvValue(fv), value_1);
        }
        if (fvField(fv) == "field_2")
        {
            EXPECT_EQ(fvValue(fv), value_2);
        }
    }
    cout << endl;

    cout << "- Step 6. DEL and GET_TABLE_CONTENT" << endl;
    cout << "Delete key [b]" << endl;
    t.del(key_2);
    t.getContent(tuples);

    EXPECT_EQ(tuples.size(), unsigned(0));

    cout << "- Step 7. hset and hget" << endl;
    string key = "k";
    string field_1 = "f1";
    string value_1_set = "v1";
    string field_2 = "f2";
    string value_2_set = "v2";
    string field_empty = "";
    string value_empty = "";
    t.hset(key, field_1, value_1_set);
    t.hset(key, field_2, value_2_set);
    t.hset(key, field_empty, value_empty);

    string value_got;
    t.hget(key, field_1, value_got);
    EXPECT_EQ(value_1_set, value_got);

    t.hget(key, field_2, value_got);
    EXPECT_EQ(value_2_set, value_got);

    bool r = t.hget(key, field_empty, value_got);
    ASSERT_TRUE(r);
    EXPECT_EQ(value_empty, value_got);

    r = t.hget(key, "e", value_got);
    ASSERT_FALSE(r);

    cout << "Done." << endl;
}

TEST(DBConnector, multi_ns_table_test)
{
    TableNetnsTest("TABLE_NS_TEST");
}
