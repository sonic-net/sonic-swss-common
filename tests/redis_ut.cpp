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
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/selectabletimer.h"
#include "common/table.h"
#include "common/redisclient.h"

using namespace std;
using namespace swss;

#define TEST_DB             (15) // Default Redis config supports 16 databases, max DB ID is 15
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
    DBConnector db(TEST_DB, "localhost", 6379, 0);
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
    DBConnector db(TEST_DB, "localhost", 6379, 0);
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
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void TableBasicTest(string tableName)
{
    DBConnector db(TEST_DB, "localhost", 6379, 0);

    Table t(&db, tableName);

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

    cout << "Done." << endl;
}

TEST(DBConnector, RedisClient)
{
    DBConnector db(TEST_DB, "localhost", 6379, 0);

    RedisClient redic(&db);

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

    redic.hmset(key_1, values.begin(), values.end());
    redic.hmset(key_2, values.begin(), values.end());

    cout << "- Step 2. GET_TABLE_KEYS" << endl;
    auto keys = redic.keys("*");
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
        auto fvs = redic.hgetall(k);
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

    cout << "- Step 4. DEL" << endl;
    cout << "Delete key [a]" << endl;
    redic.del(key_1);

    cout << "- Step 5. GET" << endl;
    cout << "Get key [a] and key [b]" << endl;
    auto fvs = redic.hgetall(key_1);
    EXPECT_TRUE(fvs.empty());
    fvs = redic.hgetall(key_2);

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

    cout << "- Step 6. DEL and GET_TABLE_CONTENT" << endl;
    cout << "Delete key [b]" << endl;
    redic.del(key_2);
    fvs = redic.hgetall(key_2);

    EXPECT_TRUE(fvs.empty());

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
    DBConnector db(TEST_DB, "localhost", 6379, 0);
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

    DBConnector db(TEST_DB, "localhost", 6379, 0);
    NotificationProducer np(&db, "UT_REDIS_CHANNEL");

    vector<FieldValueTuple> values;
    FieldValueTuple tuple("foo", "bar");
    values.push_back(tuple);

    cout << "Starting sending notification producer" << endl;
    np.send("a", "b", values);
}

TEST(DBConnector, notifications)
{
    DBConnector db(TEST_DB, "localhost", 6379, 0);
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
    TableBasicTest("TABLE_UT_TEST");
}

TEST(Table, separator_in_table_name)
{
    std::string tableName = "TABLE_UT|TEST";

    TableBasicTest(tableName);
}

TEST(Table, table_separator_test)
{
    TableBasicTest("TABLE_UT_TEST");
}

TEST(ProducerConsumer, Prefix)
{
    std::string tableName = "tableName";

    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);

    p.set("key", values, "op", "prefix_");

    ConsumerTable c(&db, tableName);

    KeyOpFieldsValuesTuple kco;
    c.pop(kco, "prefix_");

    std::string key = kfvKey(kco);
    std::string op = kfvOp(kco);
    auto vs = kfvFieldsValues(kco);

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "op");
    EXPECT_EQ(fvField(vs[0]), "f");
    EXPECT_EQ(fvValue(vs[0]), "v");
}

TEST(ProducerConsumer, Pop)
{
    std::string tableName = "tableName";

    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);

    p.set("key", values, "op", "prefix_");

    ConsumerTable c(&db, tableName);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "op");
    EXPECT_EQ(fvField(fvs[0]), "f");
    EXPECT_EQ(fvValue(fvs[0]), "v");
}

TEST(ProducerConsumer, Pop2)
{
    std::string tableName = "tableName";

    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerTable p(&db, tableName);

    std::vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);
    p.set("key", values, "op", "prefix_");

    FieldValueTuple t2("f2", "v2");
    values.clear();
    values.push_back(t2);
    p.set("key", values, "op", "prefix_");

    ConsumerTable c(&db, tableName);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "op");
    EXPECT_EQ(fvField(fvs[0]), "f");
    EXPECT_EQ(fvValue(fvs[0]), "v");

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "op");
    EXPECT_EQ(fvField(fvs[0]), "f2");
    EXPECT_EQ(fvValue(fvs[0]), "v2");
}

TEST(ProducerConsumer, PopEmpty)
{
    std::string tableName = "tableName";

    DBConnector db(TEST_DB, "localhost", 6379, 0);

    ConsumerTable c(&db, tableName);

    std::string key;
    std::string op;
    std::vector<FieldValueTuple> fvs;

    c.pop(key, op, fvs, "prefix_");

    EXPECT_EQ(key, "");
    EXPECT_EQ(op, "");
    EXPECT_EQ(fvs.size(), 0U);
}

TEST(ProducerConsumer, ConsumerSelectWithInitData)
{
    clearDB();

    string tableName = "tableName";
    DBConnector db(TEST_DB, "localhost", 6379, 0);
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
