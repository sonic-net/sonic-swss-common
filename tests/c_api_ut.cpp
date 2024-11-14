#include <unistd.h>
#include <vector>

#include "common/c-api/consumerstatetable.h"
#include "common/c-api/dbconnector.h"
#include "common/c-api/producerstatetable.h"
#include "common/c-api/subscriberstatetable.h"
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

static void freeKeyOpFieldValuesArray(SWSSKeyOpFieldValuesArray arr) {
    for (uint64_t i = 0; i < arr.len; i++) {
        free(arr.data[i].key);
        for (uint64_t j = 0; j < arr.data[i].fieldValues.len; j++) {
            free(arr.data[i].fieldValues.data[j].field);
            SWSSString_free(arr.data[i].fieldValues.data[j].value);
        }
        SWSSFieldValueArray_free(arr.data[i].fieldValues);
    }
    SWSSKeyOpFieldValuesArray_free(arr);
}

struct SWSSStringManager {
    vector<SWSSString> m_strings;

    SWSSString makeString(const char *c_str) {
        SWSSString s = SWSSString_new_c_str(c_str);
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

    EXPECT_THROW(SWSSDBConnector_new_named("does not exist", 0, true), out_of_range);
    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);
    EXPECT_EQ(SWSSDBConnector_get(db, "mykey"), nullptr);
    EXPECT_FALSE(SWSSDBConnector_exists(db, "mykey"));

    SWSSDBConnector_set(db, "mykey", sm.makeStrRef("myval"));
    SWSSString val = SWSSDBConnector_get(db, "mykey");
    EXPECT_STREQ(SWSSStrRef_c_str((SWSSStrRef)val), "myval");
    SWSSString_free(val);
    EXPECT_TRUE(SWSSDBConnector_exists(db, "mykey"));
    EXPECT_TRUE(SWSSDBConnector_del(db, "mykey"));
    EXPECT_FALSE(SWSSDBConnector_del(db, "mykey"));

    EXPECT_FALSE(SWSSDBConnector_hget(db, "mykey", "myfield"));
    EXPECT_FALSE(SWSSDBConnector_hexists(db, "mykey", "myfield"));
    SWSSDBConnector_hset(db, "mykey", "myfield", sm.makeStrRef("myval"));
    val = SWSSDBConnector_hget(db, "mykey", "myfield");
    EXPECT_STREQ(SWSSStrRef_c_str((SWSSStrRef)val), "myval");
    SWSSString_free(val);

    EXPECT_TRUE(SWSSDBConnector_hexists(db, "mykey", "myfield"));
    EXPECT_FALSE(SWSSDBConnector_hget(db, "mykey", "notmyfield"));
    EXPECT_FALSE(SWSSDBConnector_hexists(db, "mykey", "notmyfield"));
    EXPECT_TRUE(SWSSDBConnector_hdel(db, "mykey", "myfield"));
    EXPECT_FALSE(SWSSDBConnector_hdel(db, "mykey", "myfield"));

    EXPECT_TRUE(SWSSDBConnector_flushdb(db));
    SWSSDBConnector_free(db);
}

TEST(c_api, ConsumerProducerStateTables) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);
    SWSSProducerStateTable pst = SWSSProducerStateTable_new(db, "mytable");
    SWSSConsumerStateTable cst = SWSSConsumerStateTable_new(db, "mytable", nullptr, nullptr);

    SWSSConsumerStateTable_getFd(cst);

    SWSSKeyOpFieldValuesArray arr = SWSSConsumerStateTable_pops(cst);
    ASSERT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSFieldValueTuple data[2] = {
        {.field = "myfield1", .value = sm.makeString("myvalue1")},
        {.field = "myfield2", .value = sm.makeString("myvalue2")}};
    SWSSFieldValueArray values = {
        .len = 2,
        .data = data,
    };
    SWSSProducerStateTable_set(pst, "mykey1", values);

    data[0] = {.field = "myfield3", .value = sm.makeString("myvalue3")};
    values.len = 1;
    SWSSProducerStateTable_set(pst, "mykey2", values);

    ASSERT_EQ(SWSSConsumerStateTable_readData(cst, 300, true), SWSSSelectResult_DATA);
    arr = SWSSConsumerStateTable_pops(cst);
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
  
    arr = SWSSConsumerStateTable_pops(cst);
    EXPECT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSProducerStateTable_del(pst, "mykey3");
    SWSSProducerStateTable_del(pst, "mykey4");
    SWSSProducerStateTable_del(pst, "mykey5");

    arr = SWSSConsumerStateTable_pops(cst);
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
    SWSSDBConnector_flushdb(db);
    SWSSDBConnector_free(db);
}

TEST(c_api, SubscriberStateTable) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);
    SWSSSubscriberStateTable sst = SWSSSubscriberStateTable_new(db, "mytable", nullptr, nullptr);

    SWSSSubscriberStateTable_getFd(sst);

    EXPECT_EQ(SWSSSubscriberStateTable_readData(sst, 300, true), SWSSSelectResult_TIMEOUT);
    SWSSKeyOpFieldValuesArray arr = SWSSSubscriberStateTable_pops(sst);
    EXPECT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSDBConnector_hset(db, "mytable:mykey", "myfield", sm.makeStrRef("myvalue"));
    EXPECT_EQ(SWSSSubscriberStateTable_readData(sst, 300, true), SWSSSelectResult_DATA);
    arr = SWSSSubscriberStateTable_pops(sst);
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
    SWSSDBConnector_flushdb(db);
    SWSSDBConnector_free(db);
}

TEST(c_api, ZmqConsumerProducerStateTable) {
    clearDB();
    SWSSStringManager sm;

    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);

    SWSSZmqServer srv = SWSSZmqServer_new("tcp://127.0.0.1:42312");
    SWSSZmqClient cli = SWSSZmqClient_new("tcp://127.0.0.1:42312");
    EXPECT_TRUE(SWSSZmqClient_isConnected(cli));
    SWSSZmqClient_connect(cli); // This should be idempotent/not throw

    SWSSZmqProducerStateTable pst = SWSSZmqProducerStateTable_new(db, "mytable", cli, false);
    SWSSZmqConsumerStateTable cst =
        SWSSZmqConsumerStateTable_new(db, "mytable", srv, nullptr, nullptr);

    SWSSZmqConsumerStateTable_getFd(cst);

    ASSERT_EQ(SWSSZmqConsumerStateTable_getDbConnector(cst), db);

    SWSSKeyOpFieldValuesArray arr = SWSSZmqConsumerStateTable_pops(cst);
    ASSERT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    // On flag = 0, we use the ZmqProducerStateTable
    // On flag = 1, we use the ZmqClient directly
    for (int flag = 0; flag < 2; flag++) {
        SWSSFieldValueTuple values_key1_data[2] = {{.field = "myfield1", .value = sm.makeString("myvalue1")},
                                                  {.field = "myfield2", .value = sm.makeString("myvalue2")}};
        SWSSFieldValueArray values_key1 = {
            .len = 2,
            .data = values_key1_data,
        };

        SWSSFieldValueTuple values_key2_data[1] = {{.field = "myfield3", .value = sm.makeString("myvalue3")}};
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

        ASSERT_EQ(SWSSZmqConsumerStateTable_readData(cst, 1500, true), SWSSSelectResult_DATA);
        arr = SWSSZmqConsumerStateTable_pops(cst);

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

        arr = SWSSZmqConsumerStateTable_pops(cst);
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

        ASSERT_EQ(SWSSZmqConsumerStateTable_readData(cst, 500, true), SWSSSelectResult_DATA);
        arr = SWSSZmqConsumerStateTable_pops(cst);

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

    // Server must be freed first to safely release message handlers (ZmqConsumerStateTable)
    SWSSZmqServer_free(srv);

    SWSSZmqProducerStateTable_free(pst);
    SWSSZmqConsumerStateTable_free(cst);

    SWSSZmqClient_free(cli);

    SWSSDBConnector_flushdb(db);
    SWSSDBConnector_free(db);
}
