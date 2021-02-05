#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/producertable.h"
#include "common/consumertable.h"
#include "common/notificationconsumer.h"
#include "common/notificationproducer.h"
#include "common/redisclient.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/selectabletimer.h"
#include "common/table.h"
#include "common/dbinterface.h"
#include "common/sonicv2connector.h"

using namespace std;
using namespace swss;

#define NUMBER_OF_THREADS   (64) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS     (1000)
#define MAX_FIELDS_DIV      (30) // Testing up to 30 fields objects
#define PRINT_SKIP          (10) // Print + for Producer and - for Consumer for every 100 ops

int getMaxFields(int i)
{
    return (i/MAX_FIELDS_DIV) + 1;
}

string key(int i)
{
    return string("key") + to_string(i);
}

string field(int i)
{
    return string("field") + to_string(i);
}

string value(int i)
{
    return string("value") + to_string(i);
}

bool IsDigit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

int readNumberAtEOL(const string& str)
{
    auto pos = find_if(str.begin(), str.end(), IsDigit);
    istringstream is(str.substr(pos - str.begin()));
    int ret;

    is >> ret;
    return ret;
}

void validateFields(const string& key, const vector<FieldValueTuple>& f)
{
    unsigned int maxNumOfFields = getMaxFields(readNumberAtEOL(key));
    int i = 0;
    EXPECT_EQ(maxNumOfFields, f.size());

    for (auto fv : f)
    {
        EXPECT_EQ(i, readNumberAtEOL(fvField(fv)));
        EXPECT_EQ(i, readNumberAtEOL(fvValue(fv)));
        i++;
    }
}

void producerWorker(int index)
{
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db("TEST_DB", 0, true);
    ProducerTable p(&db, tableName);

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        vector<FieldValueTuple> fields;
        int maxNumOfFields = getMaxFields(i);
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }

        if ((i % 100) == 0)
            cout << "+" << flush;

        p.set(key(i), fields);
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        p.del(key(i));
    }
}

void consumerWorker(int index)
{
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db("TEST_DB", 0, true);
    ConsumerTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    int numberOfKeysSet = 0;
    int numberOfKeyDeleted = 0;
    int ret, i = 0;
    KeyOpFieldsValuesTuple kco;

    cs.addSelectable(&c);
    while ((ret = cs.select(&selectcs)) == Select::OBJECT)
    {
        c.pop(kco);
        if (kfvOp(kco) == "SET")
        {
            numberOfKeysSet++;
            validateFields(kfvKey(kco), kfvFieldsValues(kco));
        } else
        {
            numberOfKeyDeleted++;
        }

        if ((i++ % 100) == 0)
            cout << "-" << flush;

        if ((numberOfKeysSet == NUMBER_OF_OPS) &&
            (numberOfKeyDeleted == NUMBER_OF_OPS))
            break;
    }

    EXPECT_EQ(ret, Select::OBJECT);
}

void clearDB()
{
    DBConnector db("TEST_DB", 0, true);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

// Add "useDbId" to test connector objects made with dbId/dbName
void TableBasicTest(string tableName, bool useDbId = false)
{

    DBConnector *db;
    DBConnector db1("TEST_DB", 0, true);

    int dbId = db1.getDbId();

    // Use dbId to construct a DBConnector
    DBConnector db_dup(dbId, "localhost", 6379, 0);
    cout << "db_dup separator: " << SonicDBConfig::getSeparator(&db_dup) << endl;

    if (useDbId)
    {
        db = &db_dup;
    }
    else
    {
        db = &db1;
    }

    Table t(db, tableName);

    clearDB();
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

TEST(DBConnector, RedisClientName)
{
    DBConnector db("TEST_DB", 0, true);

    string client_name = "";
    sleep(1);
    EXPECT_EQ(db.getClientName(), client_name);

    client_name = "foo";
    db.setClientName(client_name);
    sleep(1);
    EXPECT_EQ(db.getClientName(), client_name);

    client_name = "bar";
    db.setClientName(client_name);
    sleep(1);
    EXPECT_EQ(db.getClientName(), client_name);

    client_name = "foobar";
    db.setClientName(client_name);
    sleep(1);
    EXPECT_EQ(db.getClientName(), client_name);
}

TEST(DBConnector, DBInterface)
{
    DBInterface dbintf;
    dbintf.set_redis_kwargs("", "127.0.0.1", 6379);
    dbintf.connect(15, "TEST_DB");

    SonicV2Connector_Native db;
    db.connect("TEST_DB");
    db.set("TEST_DB", "key0", "field1", "value2");
    auto fvs = db.get_all("TEST_DB", "key0");
    auto rc = fvs.find("field1");
    EXPECT_NE(rc, fvs.end());
    EXPECT_EQ(rc->second, "value2");
}

TEST(DBConnector, RedisClient)
{
    DBConnector db("TEST_DB", 0, true);

    clearDB();
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

    db.hmset(key_1, values.begin(), values.end());
    db.hmset(key_2, values.begin(), values.end());

    cout << "- Step 2. GET_TABLE_KEYS" << endl;
    auto keys = db.keys("*");
    EXPECT_EQ(keys.size(), (size_t)2);

    for (auto k : keys)
    {
        cout << "Get key [" << k << "]" << flush;
        EXPECT_EQ(k.length(), (size_t)1);
    }

    cout << "- Step 3. GET_TABLE_CONTENT" << endl;

    for (auto k : keys)
    {
        cout << "Get key [" << k << "]" << flush;
        auto fvs = db.hgetall(k);
        unsigned int size_v = 3;
        EXPECT_EQ(fvs.size(), size_v);
        for (auto fv: fvs)
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

    cout << "- Step 4. HDEL single field" << endl;
    cout << "Delete field_2 under key [a]" << endl;
    int64_t rval = db.hdel(key_1, "field_2");
    EXPECT_EQ(rval, 1);

    auto fvs = db.hgetall(key_1);
    EXPECT_EQ(fvs.size(), 2);
    for (auto fv: fvs)
    {
        string value_1 = "1", value_3 = "3";
        cout << " " << fvField(fv) << ":" << fvValue(fv) << flush;
        if (fvField(fv) == "field_1")
        {
            EXPECT_EQ(fvValue(fv), value_1);
        }
        if (fvField(fv) == "field_3")
        {
            EXPECT_EQ(fvValue(fv), value_3);
        }

        ASSERT_FALSE(fvField(fv) == "2");
    }
    cout << endl;

    cout << "- Step 5. DEL" << endl;
    cout << "Delete key [a]" << endl;
    db.del(key_1);

    cout << "- Step 6. GET" << endl;
    cout << "Get key [a] and key [b]" << endl;
    fvs = db.hgetall(key_1);
    EXPECT_TRUE(fvs.empty());
    fvs = db.hgetall(key_2);

    cout << "Get key [b]" << flush;
    for (auto fv: fvs)
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

    cout << "- Step 7. HDEL multiple fields" << endl;
    cout << "Delete field_2, field_3 under key [b]" << endl;
    rval = db.hdel(key_2, vector<string>({"field_2", "field_3"}));
    EXPECT_EQ(rval, 2);

    fvs = db.hgetall(key_2);
    EXPECT_EQ(fvs.size(), 1);
    for (auto fv: fvs)
    {
        string value_1 = "1";
        cout << " " << fvField(fv) << ":" << fvValue(fv) << flush;
        if (fvField(fv) == "field_1")
        {
            EXPECT_EQ(fvValue(fv), value_1);
        }

        ASSERT_FALSE(fvField(fv) == "field_2");
        ASSERT_FALSE(fvField(fv) == "field_3");
    }
    cout << endl;

    cout << "- Step 8. DEL and GET_TABLE_CONTENT" << endl;
    cout << "Delete key [b]" << endl;
    db.del(key_2);
    fvs = db.hgetall(key_2);

    EXPECT_TRUE(fvs.empty());

    // Note: ignore deprecated compilation error in unit test
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    RedisClient client(&db);
#pragma GCC diagnostic pop
    bool rc = db.set("testkey", "testvalue");
    EXPECT_TRUE(rc);

    cout << "Done." << endl;
}

TEST(DBConnector, test)
{
    thread *producerThreads[NUMBER_OF_THREADS];
    thread *consumerThreads[NUMBER_OF_THREADS];

    clearDB();

    cout << "Starting " << NUMBER_OF_THREADS*2 << " producers and consumers on redis" << endl;
    /* Starting the consumer before the producer */
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        consumerThreads[i] = new thread(consumerWorker, i);
        producerThreads[i] = new thread(producerWorker, i);
    }

    cout << "Done. Waiting for all job to finish " << NUMBER_OF_OPS << " jobs." << endl;

    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i]->join();
        delete producerThreads[i];
        consumerThreads[i]->join();
        delete consumerThreads[i];
    }
    cout << endl << "Done." << endl;
}

TEST(DBConnector, multitable)
{
    DBConnector db("TEST_DB", 0, true);
    ConsumerTable *consumers[NUMBER_OF_THREADS];
    thread *producerThreads[NUMBER_OF_THREADS];
    KeyOpFieldsValuesTuple kco;
    Select cs;
    int numberOfKeysSet = 0;
    int numberOfKeyDeleted = 0;
    int ret = 0, i;

    clearDB();

    cout << "Starting " << NUMBER_OF_THREADS*2 << " producers and consumers on redis, using single thread for consumers and thread per producer" << endl;

    /* Starting the consumer before the producer */
    for (i = 0; i < NUMBER_OF_THREADS; i++)
    {
        consumers[i] = new ConsumerTable(&db, string("UT_REDIS_THREAD_") +
                                         to_string(i));
        producerThreads[i] = new thread(producerWorker, i);
    }

    for (i = 0; i < NUMBER_OF_THREADS; i++)
        cs.addSelectable(consumers[i]);

    while (1)
    {
        Selectable *is;

        ret = cs.select(&is);
        EXPECT_EQ(ret, Select::OBJECT);

        ((ConsumerTable *)is)->pop(kco);
        if (kfvOp(kco) == "SET")
        {
            numberOfKeysSet++;
            validateFields(kfvKey(kco), kfvFieldsValues(kco));
        } else
        {
            numberOfKeyDeleted++;
            if ((numberOfKeyDeleted % 100) == 0)
                cout << "-" << flush;
        }

        if ((numberOfKeysSet == NUMBER_OF_OPS * NUMBER_OF_THREADS) &&
            (numberOfKeyDeleted == NUMBER_OF_OPS * NUMBER_OF_THREADS))
            break;
    }

    /* Making sure threads stops execution */
    for (i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i]->join();
        delete consumers[i];
        delete producerThreads[i];
    }

    cout << endl << "Done." << endl;
}


void notificationProducer()
{
    sleep(1);

    DBConnector db("TEST_DB", 0, true);
    NotificationProducer np(&db, "UT_REDIS_CHANNEL");

    vector<FieldValueTuple> values;
    FieldValueTuple tuple("foo", "bar");
    values.push_back(tuple);

    cout << "Starting sending notification producer" << endl;
    np.send("a", "b", values);
}

TEST(DBConnector, notifications)
{
    DBConnector db("TEST_DB", 0, true);
    NotificationConsumer nc(&db, "UT_REDIS_CHANNEL");
    Select s;
    s.addSelectable(&nc);
    Selectable *sel;
    int value = 1;

    clearDB();

    thread np(notificationProducer);

    int result = s.select(&sel, 2000);
    if (result == Select::OBJECT)
    {
        cout << "Got notification from producer" << endl;

        value = 2;

        string op, data;
        vector<FieldValueTuple> values;

        nc.pop(op, data, values);

        EXPECT_EQ(op, "a");
        EXPECT_EQ(data, "b");

        auto v = values.at(0);

        EXPECT_EQ(fvField(v), "foo");
        EXPECT_EQ(fvValue(v), "bar");
    }

    np.join();
    EXPECT_EQ(value, 2);
}

void selectableEventThread(Selectable *ev, int *value)
{
    Select s;
    s.addSelectable(ev);
    Selectable *sel;

    cout << "Starting listening ... " << endl;

    int result = s.select(&sel, 2000);
    if (result == Select::OBJECT)
    {
        if (sel == ev)
        {
            cout << "Got notification" << endl;
            *value = 2;
        }
    }
}

TEST(DBConnector, selectableevent)
{
    int value = 1;
    SelectableEvent ev;
    thread t(selectableEventThread, &ev, &value);

    sleep(1);

    EXPECT_EQ(value, 1);

    ev.notify();
    t.join();

    EXPECT_EQ(value, 2);
}

TEST(DBConnector, selectabletimer)
{
    timespec interval = { .tv_sec = 1, .tv_nsec = 0 };
    SelectableTimer timer(interval);

    Select s;
    s.addSelectable(&timer);
    Selectable *sel;
    int result;

    // Wait a non started timer
    result = s.select(&sel, 2000);
    ASSERT_EQ(result, Select::TIMEOUT);

    // Wait long enough so we got timer notification first
    timer.start();
    result = s.select(&sel, 2000);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);

    // Wait short so we got select timeout first
    result = s.select(&sel, 10);
    ASSERT_EQ(result, Select::TIMEOUT);

    // Wait long enough so we got timer notification first
    result = s.select(&sel, 10000);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);

    // Reset and wait long enough so we got timer notification first
    timer.reset();
    result = s.select(&sel, 10000);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);
}

TEST(Table, basic)
{
    TableBasicTest("TABLE_UT_TEST", true);
    TableBasicTest("TABLE_UT_TEST", false);
}

TEST(Table, separator_in_table_name)
{
    std::string tableName = "TABLE_UT|TEST";

    TableBasicTest(tableName, true);
    TableBasicTest(tableName, false);
}

TEST(Table, table_separator_test)
{
    TableBasicTest("TABLE_UT_TEST", true);
    TableBasicTest("TABLE_UT_TEST", false);
}

TEST(ProducerConsumer, Prefix)
{
    std::string tableName = "tableName";

    DBConnector db("TEST_DB", 0, true);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);

    p.set("key", values, "set", "prefix_");

    ConsumerTable c(&db, tableName);

    KeyOpFieldsValuesTuple kco;
    c.pop(kco, "prefix_");

    std::string key = kfvKey(kco);
    std::string op = kfvOp(kco);
    auto vs = kfvFieldsValues(kco);

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "set");
    EXPECT_EQ(fvField(vs[0]), "f");
    EXPECT_EQ(fvValue(vs[0]), "v");
}

TEST(ProducerConsumer, Pop)
{
    std::string tableName = "tableName";

    DBConnector db("TEST_DB", 0, true);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);

    p.set("key", values, "set", "prefix_");

    ConsumerTable c(&db, tableName);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "set");
    EXPECT_EQ(fvField(fvs[0]), "f");
    EXPECT_EQ(fvValue(fvs[0]), "v");
}

TEST(ProducerConsumer, Pop2)
{
    std::string tableName = "tableName";

    DBConnector db("TEST_DB", 0, true);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);
    p.set("key", values, "set", "prefix_");

    FieldValueTuple t2("f2", "v2");
    values.clear();
    values.push_back(t2);
    p.set("key", values, "set", "prefix_");

    ConsumerTable c(&db, tableName);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "set");
    EXPECT_EQ(fvField(fvs[0]), "f");
    EXPECT_EQ(fvValue(fvs[0]), "v");

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "set");
    EXPECT_EQ(fvField(fvs[0]), "f2");
    EXPECT_EQ(fvValue(fvs[0]), "v2");
}

TEST(ProducerConsumer, PopEmpty)
{
    std::string tableName = "tableName";

    DBConnector db("TEST_DB", 0, true);

    ConsumerTable c(&db, tableName);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "");
    EXPECT_EQ(op, "");
    EXPECT_EQ(fvs.size(), 0U);
}

TEST(ProducerConsumer, PopNoModify)
{
    clearDB();

    std::string tableName = "tableName";

    DBConnector db("TEST_DB", 0, true);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple fv("f", "v");
    values.push_back(fv);

    p.set("key", values, "set");

    ConsumerTable c(&db, tableName);

    c.setModifyRedis(false);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs); //, "prefixNoMod_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "set");
    EXPECT_EQ(fvField(fvs[0]), "f");
    EXPECT_EQ(fvValue(fvs[0]), "v");

    Table t(&db, tableName);

    string value_got;
    bool r = t.hget("key", "f", value_got);

    ASSERT_FALSE(r);
}

TEST(ProducerConsumer, ConsumerSelectWithInitData)
{
    clearDB();

    string tableName = "tableName";
    DBConnector db("TEST_DB", 0, true);
    ProducerTable p(&db, tableName);

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        vector<FieldValueTuple> fields;
        int maxNumOfFields = getMaxFields(i);
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        if ((i % 100) == 0)
            cout << "+" << flush;

        p.set(key(i), fields);
    }

    ConsumerTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    int ret, i = 0;
    KeyOpFieldsValuesTuple kco;

    cs.addSelectable(&c);
    int numberOfKeysSet = 0;
    while ((ret = cs.select(&selectcs)) == Select::OBJECT)
    {
        c.pop(kco);
        EXPECT_EQ(kfvOp(kco), "SET");
        numberOfKeysSet++;
        validateFields(kfvKey(kco), kfvFieldsValues(kco));

        if ((i++ % 100) == 0)
            cout << "-" << flush;

        if (numberOfKeysSet == NUMBER_OF_OPS)
            break;
    }

    /* Second select operation */
    {
        ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }

    for (i = 0; i < NUMBER_OF_OPS; i++)
    {
        p.del(key(i));
        if ((i % 100) == 0)
            cout << "+" << flush;
    }

    int numberOfKeyDeleted = 0;
    while ((ret = cs.select(&selectcs)) == Select::OBJECT)
    {
        c.pop(kco);
        EXPECT_EQ(kfvOp(kco), "DEL");
        numberOfKeyDeleted++;

        if ((i++ % 100) == 0)
            cout << "-" << flush;

        if (numberOfKeyDeleted == NUMBER_OF_OPS)
            break;
    }
    /* check select operation again */
    {
        ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }

    EXPECT_LE(numberOfKeysSet, numberOfKeyDeleted);

    cout << "Done. Waiting for all job to finish " << NUMBER_OF_OPS << " jobs." << endl;

    cout << endl << "Done." << endl;
}

TEST(Select, resultToString)
{
    auto t = Select::resultToString(Select::TIMEOUT);

    ASSERT_EQ(t, "TIMEOUT");

    auto o = Select::resultToString(Select::OBJECT);

    ASSERT_EQ(o, "OBJECT");

    auto e = Select::resultToString(Select::ERROR);

    ASSERT_EQ(e, "ERROR");

    auto u = Select::resultToString(5);

    ASSERT_EQ(u, "UNKNOWN");
}

TEST(Connector, hmset)
{
    DBConnector db("TEST_DB", 0, true);

    // test empty multi hash
    db.hmset({});
}
