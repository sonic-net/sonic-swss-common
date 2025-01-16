#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

#include "common/c-api/consumerstatetable.h"
#include "common/c-api/dbconnector.h"
#include "common/c-api/producerstatetable.h"
#include "common/c-api/result.h"
#include "common/c-api/subscriberstatetable.h"
#include "common/c-api/table.h"
#include "common/c-api/util.h"
#include "common/c-api/zmqclient.h"
#include "common/c-api/zmqconsumerstatetable.h"
#include "common/c-api/zmqproducerstatetable.h"
#include "common/c-api/zmqserver.h"
#include "common/select.h"
#include "common/subscriberstatetable.h"
#include "gtest/gtest.h"

using namespace std;
using namespace swss;

static void clearDB() {
    DBConnector db("TEST_DB", 0, true);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

static void sortKfvs(vector<KeyOpFieldsValuesTuple> &kfvs) {
    sort(kfvs.begin(), kfvs.end(),
         [](const KeyOpFieldsValuesTuple &a, const KeyOpFieldsValuesTuple &b) {
             return kfvKey(a) < kfvKey(b);
         });

    for (auto &kfv : kfvs) {
        auto &fvs = kfvFieldsValues(kfv);
        sort(fvs.begin(), fvs.end(),
             [](const pair<string, string> &a, const pair<string, string> &b) {
                 return a.first < b.first;
             });
    }
}

#define free(x) std::free(const_cast<void *>(reinterpret_cast<const void *>(x)));

static void freeFieldValuesArray(SWSSFieldValueArray arr) {
    for (uint64_t i = 0; i < arr.len; i++) {
        free(arr.data[i].field);
        SWSSString_free(arr.data[i].value);
    }
    SWSSFieldValueArray_free(arr);
}

static void freeKeyOpFieldValuesArray(SWSSKeyOpFieldValuesArray arr) {
    for (uint64_t i = 0; i < arr.len; i++) {
        free(arr.data[i].key);
        freeFieldValuesArray(arr.data[i].fieldValues);
    }
    SWSSKeyOpFieldValuesArray_free(arr);
}

struct SWSSStringManager {
    vector<SWSSString> m_strings;
    bool use_c_str = false;

    SWSSString makeString(const char *c_str) {
        use_c_str = !use_c_str;

        SWSSString s;
        if (use_c_str) {
            s = SWSSString_new_c_str(c_str);
        } else {
            s = SWSSString_new(c_str, strlen(c_str));
        }
        m_strings.push_back(s);
        return s;
    }

    SWSSStrRef makeStrRef(const char *c_str) {
        return (SWSSStrRef)makeString(c_str);
    }

    ~SWSSStringManager() {
        for (SWSSString s : m_strings)
            SWSSString_free(s);
    }
};

TEST(c_api, DBConnector) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db;
    SWSSDBConnector_new_named("TEST_DB", 1000, true, &db);

    SWSSString val;
    SWSSDBConnector_get(db, "mykey", &val);
    EXPECT_EQ(val, nullptr);

    int8_t exists;
    SWSSDBConnector_exists(db, "mykey", &exists);
    EXPECT_FALSE(exists);

    SWSSDBConnector_set(db, "mykey", sm.makeStrRef("myval"));
    SWSSDBConnector_get(db, "mykey", &val);
    EXPECT_STREQ(SWSSStrRef_c_str((SWSSStrRef)val), "myval");
    SWSSString_free(val);

    SWSSDBConnector_exists(db, "mykey", &exists);
    EXPECT_TRUE(exists);

    int8_t status;
    SWSSDBConnector_del(db, "mykey", &status);
    EXPECT_TRUE(status);

    SWSSDBConnector_del(db, "mykey", &status);
    EXPECT_FALSE(status);

    SWSSDBConnector_hget(db, "mykey", "myfield", &val);
    EXPECT_EQ(val, nullptr);

    SWSSDBConnector_hexists(db, "mykey", "myfield", &exists);
    EXPECT_FALSE(exists);

    SWSSDBConnector_hset(db, "mykey", "myfield", sm.makeStrRef("myval"));
    SWSSDBConnector_hget(db, "mykey", "myfield", &val);
    EXPECT_STREQ(SWSSStrRef_c_str((SWSSStrRef)val), "myval");
    SWSSString_free(val);

    SWSSDBConnector_hexists(db, "mykey", "myfield", &exists);
    EXPECT_TRUE(exists);

    SWSSDBConnector_hget(db, "mykey", "notmyfield", &val);
    EXPECT_EQ(val, nullptr);

    SWSSDBConnector_hexists(db, "mykey", "notmyfield", &exists);
    EXPECT_FALSE(exists);

    SWSSDBConnector_hdel(db, "mykey", "myfield", &status);
    EXPECT_TRUE(status);

    SWSSDBConnector_hdel(db, "mykey", "myfield", &status);
    EXPECT_FALSE(status);

    SWSSDBConnector_flushdb(db, &status);
    EXPECT_TRUE(status);

    SWSSDBConnector_free(db);
}

TEST(c_api, Table) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db;
    SWSSDBConnector_new_named("TEST_DB", 1000, true, &db);
    SWSSTable tbl;
    SWSSTable_new(db, "mytable", &tbl);

    SWSSFieldValueArray fvs;
    SWSSString ss;
    int8_t exists;
    SWSSTable_get(tbl, "mykey", &fvs, &exists);
    EXPECT_FALSE(exists);
    SWSSTable_hget(tbl, "mykey", "myfield", &ss, &exists);
    EXPECT_FALSE(exists);

    SWSSStringArray keys;
    SWSSTable_getKeys(tbl, &keys);
    EXPECT_EQ(keys.len, 0);
    SWSSStringArray_free(keys);

    SWSSTable_hset(tbl, "mykey", "myfield", sm.makeStrRef("myvalue"));
    SWSSTable_getKeys(tbl, &keys);
    ASSERT_EQ(keys.len, 1);
    EXPECT_STREQ(keys.data[0], "mykey");
    free(keys.data[0]);
    SWSSStringArray_free(keys);

    SWSSTable_hget(tbl, "mykey", "myfield", &ss, &exists);
    ASSERT_TRUE(exists);
    EXPECT_STREQ(SWSSStrRef_c_str((SWSSStrRef)ss), "myvalue");
    SWSSString_free(ss);

    SWSSTable_hdel(tbl, "mykey", "myfield");
    SWSSTable_hget(tbl, "mykey", "myfield", &ss, &exists);
    EXPECT_FALSE(exists);

    SWSSFieldValueTuple data[2] = {{.field = "myfield1", .value = sm.makeString("myvalue1")},
                                   {.field = "myfield2", .value = sm.makeString("myvalue2")}};
    fvs.len = 2;
    fvs.data = data;
    SWSSTable_set(tbl, "mykey", fvs);

    SWSSTable_get(tbl, "mykey", &fvs, &exists);
    ASSERT_TRUE(exists);
    ASSERT_EQ(fvs.len, 2);
    EXPECT_STREQ(data[0].field, fvs.data[0].field);
    EXPECT_STREQ(data[1].field, fvs.data[1].field);
    freeFieldValuesArray(fvs);

    SWSSTable_del(tbl, "mykey");
    SWSSTable_getKeys(tbl, &keys);
    EXPECT_EQ(keys.len, 0);
    SWSSStringArray_free(keys);

    SWSSTable_free(tbl);
    SWSSDBConnector_free(db);
}

TEST(c_api, ConsumerProducerStateTables) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db;
    SWSSDBConnector_new_named("TEST_DB", 1000, true, &db);
    SWSSProducerStateTable pst;
    SWSSProducerStateTable_new(db, "mytable", &pst);
    SWSSConsumerStateTable cst;
    SWSSConsumerStateTable_new(db, "mytable", nullptr, nullptr, &cst);

    uint32_t fd;
    SWSSConsumerStateTable_getFd(cst, &fd);

    SWSSKeyOpFieldValuesArray arr;
    SWSSConsumerStateTable_pops(cst, &arr);
    ASSERT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSFieldValueTuple data[2] = {{.field = "myfield1", .value = sm.makeString("myvalue1")},
                                   {.field = "myfield2", .value = sm.makeString("myvalue2")}};
    SWSSFieldValueArray values = {
        .len = 2,
        .data = data,
    };
    SWSSProducerStateTable_set(pst, "mykey1", values);

    data[0] = {.field = "myfield3", .value = sm.makeString("myvalue3")};
    values.len = 1;
    SWSSProducerStateTable_set(pst, "mykey2", values);

    SWSSSelectResult result;
    SWSSConsumerStateTable_readData(cst, 300, true, &result);
    SWSSConsumerStateTable_pops(cst, &arr);
    vector<KeyOpFieldsValuesTuple> kfvs = takeKeyOpFieldValuesArray(arr);
    sortKfvs(kfvs);
    freeKeyOpFieldValuesArray(arr);

    ASSERT_EQ(kfvs.size(), 2);
    EXPECT_EQ(kfvKey(kfvs[0]), "mykey1");
    EXPECT_EQ(kfvOp(kfvs[0]), "SET");
    vector<pair<string, string>> &fieldValues0 = kfvFieldsValues(kfvs[0]);
    ASSERT_EQ(fieldValues0.size(), 2);
    EXPECT_EQ(fieldValues0[0].first, "myfield1");
    EXPECT_EQ(fieldValues0[0].second, "myvalue1");
    EXPECT_EQ(fieldValues0[1].first, "myfield2");
    EXPECT_EQ(fieldValues0[1].second, "myvalue2");

    EXPECT_EQ(kfvKey(kfvs[1]), "mykey2");
    EXPECT_EQ(kfvOp(kfvs[1]), "SET");
    vector<pair<string, string>> &fieldValues1 = kfvFieldsValues(kfvs[1]);
    ASSERT_EQ(fieldValues1.size(), 1);
    EXPECT_EQ(fieldValues1[0].first, "myfield3");
    EXPECT_EQ(fieldValues1[0].second, "myvalue3");

    SWSSConsumerStateTable_pops(cst, &arr);
    EXPECT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSProducerStateTable_del(pst, "mykey3");
    SWSSProducerStateTable_del(pst, "mykey4");
    SWSSProducerStateTable_del(pst, "mykey5");

    SWSSConsumerStateTable_pops(cst, &arr);
    kfvs = takeKeyOpFieldValuesArray(arr);
    sortKfvs(kfvs);
    freeKeyOpFieldValuesArray(arr);

    ASSERT_EQ(kfvs.size(), 3);
    EXPECT_EQ(kfvKey(kfvs[0]), "mykey3");
    EXPECT_EQ(kfvOp(kfvs[0]), "DEL");
    EXPECT_EQ(kfvFieldsValues(kfvs[0]).size(), 0);
    EXPECT_EQ(kfvKey(kfvs[1]), "mykey4");
    EXPECT_EQ(kfvOp(kfvs[1]), "DEL");
    EXPECT_EQ(kfvFieldsValues(kfvs[1]).size(), 0);
    EXPECT_EQ(kfvKey(kfvs[2]), "mykey5");
    EXPECT_EQ(kfvOp(kfvs[2]), "DEL");
    EXPECT_EQ(kfvFieldsValues(kfvs[2]).size(), 0);

    SWSSProducerStateTable_free(pst);
    SWSSConsumerStateTable_free(cst);
    int8_t flushStatus;
    SWSSDBConnector_flushdb(db, &flushStatus);
    SWSSDBConnector_free(db);
}

TEST(c_api, SubscriberStateTable) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db;
    SWSSDBConnector_new_named("TEST_DB", 1000, true, &db);
    SWSSSubscriberStateTable sst;
    SWSSSubscriberStateTable_new(db, "mytable", nullptr, nullptr, &sst);

    uint32_t fd;
    SWSSSubscriberStateTable_getFd(sst, &fd);

    SWSSSelectResult result;
    SWSSSubscriberStateTable_readData(sst, 300, true, &result);
    EXPECT_EQ(result, SWSSSelectResult_TIMEOUT);
    SWSSKeyOpFieldValuesArray arr;
    SWSSSubscriberStateTable_pops(sst, &arr);
    EXPECT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSDBConnector_hset(db, "mytable:mykey", "myfield", sm.makeStrRef("myvalue"));
    SWSSSubscriberStateTable_readData(sst, 300, true, &result);
    EXPECT_EQ(result, SWSSSelectResult_DATA);
    SWSSSubscriberStateTable_pops(sst, &arr);
    vector<KeyOpFieldsValuesTuple> kfvs = takeKeyOpFieldValuesArray(arr);
    sortKfvs(kfvs);
    freeKeyOpFieldValuesArray(arr);

    ASSERT_EQ(kfvs.size(), 1);
    EXPECT_EQ(kfvKey(kfvs[0]), "mykey");
    EXPECT_EQ(kfvOp(kfvs[0]), "SET");
    ASSERT_EQ(kfvFieldsValues(kfvs[0]).size(), 1);
    EXPECT_EQ(kfvFieldsValues(kfvs[0])[0].first, "myfield");
    EXPECT_EQ(kfvFieldsValues(kfvs[0])[0].second, "myvalue");

    SWSSSubscriberStateTable_free(sst);
    int8_t flushStatus;
    SWSSDBConnector_flushdb(db, &flushStatus);
    SWSSDBConnector_free(db);
}

TEST(c_api, ZmqConsumerProducerStateTable) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db;
    SWSSDBConnector_new_named("TEST_DB", 1000, true, &db);

    SWSSZmqServer srv;
    SWSSZmqServer_new("tcp://127.0.0.1:42312", &srv);
    SWSSZmqClient cli;
    SWSSZmqClient_new("tcp://127.0.0.1:42312", &cli);
    int8_t isConnected;
    SWSSZmqClient_isConnected(cli, &isConnected);
    ASSERT_TRUE(isConnected);
    SWSSZmqClient_connect(cli);

    SWSSZmqProducerStateTable pst;
    SWSSZmqProducerStateTable_new(db, "mytable", cli, false, &pst);
    SWSSZmqConsumerStateTable cst;
    SWSSZmqConsumerStateTable_new(db, "mytable", srv, nullptr, nullptr, &cst);

    uint32_t fd;
    SWSSZmqConsumerStateTable_getFd(cst, &fd);

    const SWSSDBConnectorOpaque *dbConnector;
    SWSSZmqConsumerStateTable_getDbConnector(cst, &dbConnector);
    ASSERT_EQ(dbConnector, db);

    SWSSKeyOpFieldValuesArray arr;
    SWSSZmqConsumerStateTable_pops(cst, &arr);
    ASSERT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    for (int flag = 0; flag < 2; flag++) {
        SWSSFieldValueTuple values_key1_data[2] = {
            {.field = "myfield1", .value = sm.makeString("myvalue1")},
            {.field = "myfield2", .value = sm.makeString("myvalue2")}};
        SWSSFieldValueArray values_key1 = {
            .len = 2,
            .data = values_key1_data,
        };

        SWSSFieldValueTuple values_key2_data[1] = {
            {.field = "myfield3", .value = sm.makeString("myvalue3")}};
        SWSSFieldValueArray values_key2 = {
            .len = 1,
            .data = values_key2_data,
        };

        SWSSKeyOpFieldValues arr_data[2] = {
            {.key = "mykey1", .operation = SWSSKeyOperation_SET, .fieldValues = values_key1},
            {.key = "mykey2", .operation = SWSSKeyOperation_SET, .fieldValues = values_key2}};
        arr = {.len = 2, .data = arr_data};

        if (flag == 0)
            for (uint64_t i = 0; i < arr.len; i++)
                SWSSZmqProducerStateTable_set(pst, arr.data[i].key, arr.data[i].fieldValues);
        else
            SWSSZmqClient_sendMsg(cli, "TEST_DB", "mytable", arr);

        SWSSSelectResult result;
        SWSSZmqConsumerStateTable_readData(cst, 1500, true, &result);
        SWSSZmqConsumerStateTable_pops(cst, &arr);

        vector<KeyOpFieldsValuesTuple> kfvs = takeKeyOpFieldValuesArray(arr);
        sortKfvs(kfvs);
        freeKeyOpFieldValuesArray(arr);

        ASSERT_EQ(kfvs.size(), 2);
        EXPECT_EQ(kfvKey(kfvs[0]), "mykey1");
        EXPECT_EQ(kfvOp(kfvs[0]), "SET");
        vector<pair<string, string>> &fieldValues0 = kfvFieldsValues(kfvs[0]);
        ASSERT_EQ(fieldValues0.size(), 2);
        EXPECT_EQ(fieldValues0[0].first, "myfield1");
        EXPECT_EQ(fieldValues0[0].second, "myvalue1");
        EXPECT_EQ(fieldValues0[1].first, "myfield2");
        EXPECT_EQ(fieldValues0[1].second, "myvalue2");

        EXPECT_EQ(kfvKey(kfvs[1]), "mykey2");
        EXPECT_EQ(kfvOp(kfvs[1]), "SET");
        vector<pair<string, string>> &fieldValues1 = kfvFieldsValues(kfvs[1]);
        ASSERT_EQ(fieldValues1.size(), 1);
        EXPECT_EQ(fieldValues1[0].first, "myfield3");
        EXPECT_EQ(fieldValues1[0].second, "myvalue3");

        SWSSZmqConsumerStateTable_pops(cst, &arr);
        ASSERT_EQ(arr.len, 0);
        freeKeyOpFieldValuesArray(arr);

        arr_data[0] = {.key = "mykey3", .operation = SWSSKeyOperation_DEL, .fieldValues = {}};
        arr_data[1] = {.key = "mykey4", .operation = SWSSKeyOperation_DEL, .fieldValues = {}};
        arr = {.len = 2, .data = arr_data};

        if (flag == 0)
            for (uint64_t i = 0; i < arr.len; i++)
                SWSSZmqProducerStateTable_del(pst, arr.data[i].key);
        else
            SWSSZmqClient_sendMsg(cli, "TEST_DB", "mytable", arr);

        SWSSZmqConsumerStateTable_readData(cst, 500, true, &result);
        SWSSZmqConsumerStateTable_pops(cst, &arr);

        kfvs = takeKeyOpFieldValuesArray(arr);
        sortKfvs(kfvs);
        freeKeyOpFieldValuesArray(arr);

        ASSERT_EQ(kfvs.size(), 2);
        EXPECT_EQ(kfvKey(kfvs[0]), "mykey3");
        EXPECT_EQ(kfvOp(kfvs[0]), "DEL");
        EXPECT_EQ(kfvFieldsValues(kfvs[0]).size(), 0);
        EXPECT_EQ(kfvKey(kfvs[1]), "mykey4");
        EXPECT_EQ(kfvOp(kfvs[1]), "DEL");
        EXPECT_EQ(kfvFieldsValues(kfvs[1]).size(), 0);
    }

    SWSSZmqServer_free(srv);

    SWSSZmqProducerStateTable_free(pst);
    SWSSZmqConsumerStateTable_free(cst);

    SWSSZmqClient_free(cli);

    int8_t flushStatus;
    SWSSDBConnector_flushdb(db, &flushStatus);
    SWSSDBConnector_free(db);
}

TEST(c_api, exceptions) {
    SWSSDBConnector db = nullptr;
    SWSSResult result = SWSSDBConnector_new_tcp(0, "127.0.0.1", 1, 1000, &db);
    ASSERT_EQ(db, nullptr);
    ASSERT_NE(result.exception, SWSSException_None);
    ASSERT_NE(result.location, nullptr);
    ASSERT_NE(result.message, nullptr);

    const char *cstr = SWSSStrRef_c_str((SWSSStrRef)result.location);
    EXPECT_EQ(SWSSStrRef_length((SWSSStrRef)result.location), strlen(cstr));
    printf("Exception location: %s\n", cstr);
    cstr = SWSSStrRef_c_str((SWSSStrRef)result.message);
    EXPECT_EQ(SWSSStrRef_length((SWSSStrRef)result.message), strlen(cstr));
    printf("Exception message: %s\n", cstr);

    SWSSString_free(result.location);
    SWSSString_free(result.message);
}
