#include <cstdlib>
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

template <typename T> static void free(const T *ptr) {
    std::free(const_cast<void *>(reinterpret_cast<const void *>(ptr)));
}

static void freeKeyOpFieldValuesArray(SWSSKeyOpFieldValuesArray arr) {
    for (uint64_t i = 0; i < arr.len; i++) {
        free(arr.data[i].key);
        free(arr.data[i].operation);
        for (uint64_t j = 0; j < arr.data[i].fieldValues.len; j++) {
            free(arr.data[i].fieldValues.data[j].field);
            free(arr.data[i].fieldValues.data[j].value);
        }
        free(arr.data[i].fieldValues.data);
    }
    free(arr.data);
}

TEST(c_api, DBConnector) {
    clearDB();

    EXPECT_THROW(SWSSDBConnector_new_named("does not exist", 0, true), out_of_range);
    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);
    EXPECT_FALSE(SWSSDBConnector_get(db, "mykey"));
    EXPECT_FALSE(SWSSDBConnector_exists(db, "mykey"));
    SWSSDBConnector_set(db, "mykey", "myval");
    const char *val = SWSSDBConnector_get(db, "mykey");
    EXPECT_STREQ(val, "myval");
    free(val);
    EXPECT_TRUE(SWSSDBConnector_exists(db, "mykey"));
    EXPECT_TRUE(SWSSDBConnector_del(db, "mykey"));
    EXPECT_FALSE(SWSSDBConnector_del(db, "mykey"));

    EXPECT_FALSE(SWSSDBConnector_hget(db, "mykey", "myfield"));
    EXPECT_FALSE(SWSSDBConnector_hexists(db, "mykey", "myfield"));
    SWSSDBConnector_hset(db, "mykey", "myfield", "myval");
    val = SWSSDBConnector_hget(db, "mykey", "myfield");
    EXPECT_STREQ(val, "myval");
    free(val);
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

    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);
    SWSSProducerStateTable pst = SWSSProducerStateTable_new(db, "mytable");
    SWSSConsumerStateTable cst = SWSSConsumerStateTable_new(db, "mytable", nullptr, nullptr);

    SWSSKeyOpFieldValuesArray arr = SWSSConsumerStateTable_pops(cst);
    ASSERT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSFieldValuePair data[2] = {{.field = "myfield1", .value = "myvalue1"},
                                  {.field = "myfield2", .value = "myvalue2"}};
    SWSSFieldValueArray values = {
        .len = 2,
        .data = data,
    };
    SWSSProducerStateTable_set(pst, "mykey1", values);

    data[0] = {.field = "myfield3", .value = "myvalue3"};
    values.len = 1;
    SWSSProducerStateTable_set(pst, "mykey2", values);

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

    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);
    SWSSSubscriberStateTable sst = SWSSSubscriberStateTable_new(db, "mytable", nullptr, nullptr);

    EXPECT_EQ(SWSSSubscriberStateTable_readData(sst, 300), SWSSSelectResult_TIMEOUT);
    EXPECT_FALSE(SWSSSubscriberStateTable_hasData(sst));
    SWSSKeyOpFieldValuesArray arr = SWSSSubscriberStateTable_pops(sst);
    EXPECT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    SWSSDBConnector_hset(db, "mytable:mykey", "myfield", "myvalue");
    EXPECT_EQ(SWSSSubscriberStateTable_readData(sst, 300), SWSSSelectResult_DATA);
    EXPECT_EQ(SWSSSubscriberStateTable_readData(sst, 300), SWSSSelectResult_TIMEOUT);
    EXPECT_TRUE(SWSSSubscriberStateTable_hasData(sst));

    arr = SWSSSubscriberStateTable_pops(sst);
    vector<KeyOpFieldsValuesTuple> kfvs = takeKeyOpFieldValuesArray(arr);
    sortKfvs(kfvs);
    freeKeyOpFieldValuesArray(arr);

    EXPECT_FALSE(SWSSSubscriberStateTable_hasData(sst));
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

    SWSSDBConnector db = SWSSDBConnector_new_named("TEST_DB", 1000, true);

    SWSSZmqServer srv = SWSSZmqServer_new("tcp://127.0.0.1:42312");
    SWSSZmqClient cli = SWSSZmqClient_new("tcp://127.0.0.1:42312");
    EXPECT_TRUE(SWSSZmqClient_isConnected(cli));
    SWSSZmqClient_connect(cli); // This should be idempotent/not throw

    SWSSZmqProducerStateTable pst = SWSSZmqProducerStateTable_new(db, "mytable", cli, false);
    SWSSZmqConsumerStateTable cst =
        SWSSZmqConsumerStateTable_new(db, "mytable", srv, nullptr, nullptr);

    ASSERT_EQ(SWSSZmqConsumerStateTable_getDbConnector(cst), db);

    EXPECT_FALSE(SWSSZmqConsumerStateTable_hasData(cst));
    EXPECT_FALSE(SWSSZmqConsumerStateTable_hasCachedData(cst));
    EXPECT_FALSE(SWSSZmqConsumerStateTable_initializedWithData(cst));
    SWSSKeyOpFieldValuesArray arr = SWSSZmqConsumerStateTable_pops(cst);
    ASSERT_EQ(arr.len, 0);
    freeKeyOpFieldValuesArray(arr);

    // On flag = 0, we use the ZmqProducerStateTable
    // On flag = 1, we use the ZmqClient directly
    for (int flag = 0; flag < 2; flag++) {
        SWSSFieldValuePair values_key1_data[2] = {{.field = "myfield1", .value = "myvalue1"},
                                                  {.field = "myfield2", .value = "myvalue2"}};
        SWSSFieldValueArray values_key1 = {
            .len = 2,
            .data = values_key1_data,
        };

        SWSSFieldValuePair values_key2_data[1] = {{.field = "myfield3", .value = "myvalue3"}};
        SWSSFieldValueArray values_key2 = {
            .len = 1,
            .data = values_key2_data,
        };

        SWSSKeyOpFieldValues arr_data[2] = {
            {.key = "mykey1", .operation = "SET", .fieldValues = values_key1},
            {.key = "mykey2", .operation = "SET", .fieldValues = values_key2}};
        arr = {.len = 2, .data = arr_data};

        if (flag == 0)
            for (uint64_t i = 0; i < arr.len; i++)
                SWSSZmqProducerStateTable_set(pst, arr.data[i].key, arr.data[i].fieldValues);
        else
            SWSSZmqClient_sendMsg(cli, "TEST_DB", "mytable", &arr);

        ASSERT_EQ(SWSSZmqConsumerStateTable_readData(cst, 500), SWSSSelectResult_DATA);
        EXPECT_TRUE(SWSSZmqConsumerStateTable_hasData(cst));
        EXPECT_TRUE(SWSSZmqConsumerStateTable_hasCachedData(cst));
        arr = SWSSZmqConsumerStateTable_pops(cst);
        EXPECT_FALSE(SWSSZmqConsumerStateTable_hasData(cst));
        EXPECT_FALSE(SWSSZmqConsumerStateTable_hasCachedData(cst));
        EXPECT_EQ(SWSSZmqConsumerStateTable_readData(cst, 500), SWSSSelectResult_TIMEOUT);

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

        EXPECT_FALSE(SWSSZmqConsumerStateTable_hasData(cst));
        arr = SWSSZmqConsumerStateTable_pops(cst);
        ASSERT_EQ(arr.len, 0);
        freeKeyOpFieldValuesArray(arr);

        arr_data[0] = {.key = "mykey3", .operation = "DEL", .fieldValues = {}};
        arr_data[1] = {.key = "mykey4", .operation = "DEL", .fieldValues = {}};
        arr = { .len = 2, .data = arr_data };

        if (flag == 0)
            for (uint64_t i = 0; i < arr.len; i++)
                SWSSZmqProducerStateTable_del(pst, arr.data[i].key);
        else
            SWSSZmqClient_sendMsg(cli, "TEST_DB", "mytable", &arr);

        ASSERT_EQ(SWSSZmqConsumerStateTable_readData(cst, 500), SWSSSelectResult_DATA);
        EXPECT_TRUE(SWSSZmqConsumerStateTable_hasData(cst));
        EXPECT_TRUE(SWSSZmqConsumerStateTable_hasCachedData(cst));
        arr = SWSSZmqConsumerStateTable_pops(cst);
        EXPECT_FALSE(SWSSZmqConsumerStateTable_hasData(cst));
        EXPECT_FALSE(SWSSZmqConsumerStateTable_hasCachedData(cst));
        EXPECT_EQ(SWSSZmqConsumerStateTable_readData(cst, 500), SWSSSelectResult_TIMEOUT);

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
