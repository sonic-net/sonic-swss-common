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
#include "common/table.h"

using namespace std;
using namespace swss;

#define TEST_VIEW            (7)
#define NUMBER_OF_THREADS   (64) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS     (1000)
#define MAX_FIELDS_DIV      (30) // Testing up to 30 fields objects
#define PRINT_SKIP          (10) // Print + for Producer and - for Consumer for every 100 ops

static int getMaxFields(int i)
{
    return (i/MAX_FIELDS_DIV) + 1;
}

static string key(int i)
{
    return string("key") + to_string(i);
}

static string field(int i)
{
    return string("field") + to_string(i);
}

static string value(int i)
{
    return string("value") + to_string(i);
}

static bool IsDigit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

static int readNumberAtEOL(const string& str)
{
    auto pos = find_if(str.begin(), str.end(), IsDigit);
    istringstream is(str.substr(pos - str.begin()));
    int ret;

    is >> ret;
    return ret;
}

static void validateFields(const string& key, const vector<FieldValueTuple>& f)
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

static void producerWorker(int index)
{
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    RedisPipeline pipeline(&db);
    ProducerTable p(&pipeline, tableName, true);

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

static void consumerWorker(int index)
{
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    ConsumerTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    int tmpfd;
    int numberOfKeysSet = 0;
    int numberOfKeyDeleted = 0;
    int ret, i = 0;
    KeyOpFieldsValuesTuple kco;

    cs.addSelectable(&c);
    while ((ret = cs.select(&selectcs, &tmpfd)) == Select::OBJECT)
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

    EXPECT_EQ(ret, Selectable::DATA);
}

static void clearDB()
{
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

TEST(DBConnector, piped_test)
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

TEST(DBConnector, piped_multitable)
{
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
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
        int fd;

        ret = cs.select(&is, &fd);
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


static void notificationProducer()
{
    sleep(1);

    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    NotificationProducer np(&db, "UT_REDIS_CHANNEL");

    vector<FieldValueTuple> values;
    FieldValueTuple tuple("foo", "bar");
    values.push_back(tuple);

    cout << "Starting sending notification producer" << endl;
    np.send("a", "b", values);
}

TEST(DBConnector, piped_notifications)
{
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    NotificationConsumer nc(&db, "UT_REDIS_CHANNEL");
    Select s;
    s.addSelectable(&nc);
    Selectable *sel;
    int fd, value = 1;

    clearDB();

    thread np(notificationProducer);

    int result = s.select(&sel, &fd, 2000);
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

static void selectableEventThread(Selectable *ev, int *value)
{
    Select s;
    s.addSelectable(ev);
    Selectable *sel;
    int fd;

    cout << "Starting listening ... " << endl;

    int result = s.select(&sel, &fd, 2000);
    if (result == Select::OBJECT)
    {
        if (sel == ev)
        {
            cout << "Got notification" << endl;
            *value = 2;
        }
    }
}

TEST(DBConnector, piped_selectableevent)
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

TEST(Table, piped_test)
{
    string tableName = "TABLE_UT_TEST";
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
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

    cout << "- Step 2. GET_TABLE_CONTENT" << endl;
    vector<KeyOpFieldsValuesTuple> tuples;
    t.getTableContent(tuples);

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
            if (fvField(fv) == "field_1") EXPECT_EQ(fvValue(fv), value_1);
            if (fvField(fv) == "field_2") EXPECT_EQ(fvValue(fv), value_2);
        }
        cout << endl;
    }

    cout << "- Step 3. DEL" << endl;
    cout << "Delete key [a]" << endl;
    t.del(key_1);

    cout << "- Step 4. GET" << endl;
    cout << "Get key [a] and key [b]" << endl;
    EXPECT_EQ(t.get(key_1, values), false);
    t.get(key_2, values);

    cout << "Get key [b]" << flush;
    for (auto fv: values)
    {
        string value_1 = "1", value_2 = "2";
        cout << " " << fvField(fv) << ":" << fvValue(fv) << flush;
        if (fvField(fv) == "field_1") EXPECT_EQ(fvValue(fv), value_1);
        if (fvField(fv) == "field_2") EXPECT_EQ(fvValue(fv), value_2);
    }
    cout << endl;

    cout << "- Step 5. DEL and GET_TABLE_CONTENT" << endl;
    cout << "Delete key [b]" << endl;
    t.del(key_2);
    t.getTableContent(tuples);

    EXPECT_EQ(tuples.size(), unsigned(0));

    cout << "Done." << endl;
}

TEST(ProducerConsumer, piped_Prefix)
{
    string tableName = "tableName";

    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    RedisPipeline pipeline(&db);
    ProducerTable p(&pipeline, tableName, true);

    vector<FieldValueTuple> values;

    FieldValueTuple t("f", "v");
    values.push_back(t);

    p.set("key", values, "op", "prefix_");
    p.flush();

    ConsumerTable c(&db, tableName);

    KeyOpFieldsValuesTuple kco;
    c.pop(kco, "prefix_");

    string key = kfvKey(kco);
    string op = kfvOp(kco);
    auto vs = kfvFieldsValues(kco);

    EXPECT_EQ(key, "key");
    EXPECT_EQ(op, "op");
    EXPECT_EQ(fvField(vs[0]), "f");
    EXPECT_EQ(fvValue(vs[0]), "v");
}
