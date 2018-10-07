#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/notificationconsumer.h"
#include "common/notificationproducer.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/table.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/redisclient.h"

using namespace std;
using namespace swss;

#define TEST_DB           APPL_DB // Need to test against a DB which uses a colon table name separator due to hardcoding in consumer_table_pops.lua
#define NUMBER_OF_THREADS    (64) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS      (1000)
#define MAX_FIELDS_DIV       (30) // Testing up to 30 fields objects
#define PRINT_SKIP           (10) // Print + for Producer and - for Consumer for every 100 ops

static inline int getMaxFields(int i)
{
    return (i/MAX_FIELDS_DIV) + 1;
}

static inline string key(int i)
{
    return string("key ") + to_string(i);
}

static inline string field(int i)
{
    return string("field ") + to_string(i);
}

static inline string value(int i)
{
    if (i == 0) return string(); // emtpy
    return string("value ") + to_string(i);
}

static inline bool IsDigit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

static inline int readNumberAtEOL(const string& str)
{
    if (str.empty()) return 0;
    auto pos = find_if(str.begin(), str.end(), IsDigit);
    istringstream is(str.substr(pos - str.begin()));
    int ret;
    is >> ret;
    EXPECT_TRUE(is);
    return ret;
}

static inline void validateFields(const string& key, const vector<FieldValueTuple>& f)
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
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerStateTable p(&db, tableName);

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
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    RedisClient redisClient(&db);
    ConsumerStateTable c(&db, tableName);
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

            for (auto fv : kfvFieldsValues(kco))
            {
                string val = *redisClient.hget(tableName + ":" + kfvKey(kco), fvField(fv));
                EXPECT_EQ(val, fvValue(fv));
            }
        } else if (kfvOp(kco) == "DEL")
        {
            numberOfKeyDeleted++;
            auto keys = redisClient.keys(tableName + ":" + kfvKey(kco));
            EXPECT_EQ(0UL, keys.size());
        }

        if ((i++ % 100) == 0)
            cout << "-" << flush;

        if (numberOfKeyDeleted == NUMBER_OF_OPS)
            break;
    }

    EXPECT_LE(numberOfKeysSet, numberOfKeyDeleted);
    EXPECT_EQ(ret, Select::OBJECT);
}

static inline void clearDB()
{
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

TEST(ConsumerStateTable, double_set)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerStateTable p(&db, tableName);
    string key = "TheKey";
    int maxNumOfFields = 2;

    /* First set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Second set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields * 2; j += 2)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Prepare consumer */
    ConsumerStateTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    cs.addSelectable(&c);

    /* First pop operation */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "SET");

        auto fvs = kfvFieldsValues(kco);
        EXPECT_EQ(fvs.size(), (unsigned int)(maxNumOfFields + maxNumOfFields/2));

        map<string, string> mm;
        for (auto fv: fvs)
        {
            mm[fvField(fv)] = fvValue(fv);
        }

        for (int j = 0; j < maxNumOfFields; j++)
        {
            EXPECT_EQ(mm[field(j)], value(j));
        }
        for (int j = 0; j < maxNumOfFields * 2; j += 2)
        {
            EXPECT_EQ(mm[field(j)], value(j));
        }
    }

    /* Second select operation */
    {
        int ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }

    /* State Queue should be empty */
    RedisCommand keys;
    keys.format("KEYS %s*", (c.getStateHashPrefix() + tableName).c_str());
    RedisReply r(&db, keys, REDIS_REPLY_ARRAY);
    auto qlen = r.getContext()->elements;
    EXPECT_EQ(qlen, 0U);
}

TEST(ConsumerStateTable, set_del)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerStateTable p(&db, tableName);
    string key = "TheKey";
    int maxNumOfFields = 2;

    /* Set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Del operation */
    p.del(key);

    /* Prepare consumer */
    ConsumerStateTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    cs.addSelectable(&c);

    /* First pop operation */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "DEL");

        auto fvs = kfvFieldsValues(kco);
        EXPECT_EQ(fvs.size(), 0U);
    }

    /* Second select operation */
    {
        int ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }

    /* State Queue should be empty */
    RedisCommand keys;
    keys.format("KEYS %s*", (p.getStateHashPrefix() + tableName).c_str());
    RedisReply r(&db, keys, REDIS_REPLY_ARRAY);
    auto qlen = r.getContext()->elements;
    EXPECT_EQ(qlen, 0U);
}

TEST(ConsumerStateTable, set_del_set)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerStateTable p(&db, tableName);
    string key = "TheKey";
    int maxNumOfFields = 2;

    /* First set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Del operation */
    p.del(key);

    /* Second set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields * 2; j += 2)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Prepare consumer */
    ConsumerStateTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    cs.addSelectable(&c);

    /* First pop operation */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "SET");

        auto fvs = kfvFieldsValues(kco);
        EXPECT_EQ(fvs.size(), (unsigned int)maxNumOfFields);

        map<string, string> mm;
        for (auto fv: fvs)
        {
            mm[fvField(fv)] = fvValue(fv);
        }

        for (int j = 0; j < maxNumOfFields * 2; j += 2)
        {
            EXPECT_EQ(mm[field(j)], value(j));
        }
    }

    /* Second select operation */
    {
        int ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }

    /* State Queue should be empty */
    RedisCommand keys;
    keys.format("KEYS %s*", (c.getStateHashPrefix() + tableName).c_str());
    RedisReply r(&db, keys, REDIS_REPLY_ARRAY);
    auto qlen = r.getContext()->elements;
    EXPECT_EQ(qlen, 0U);
}

TEST(ConsumerStateTable, set_pop_del_set_pop_get)
{
    clearDB();

    /* Prepare producer */
    int index = 0;
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerStateTable p(&db, tableName);
    string key = "TheKey";
    int maxNumOfFields = 2;

    /* First set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

  /* Prepare consumer */
    ConsumerStateTable c(&db, tableName);
    Select cs;
    Selectable *selectcs;
    cs.addSelectable(&c);

    /* First pop operation */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "SET");

        auto fvs = kfvFieldsValues(kco);
        EXPECT_EQ(fvs.size(), (unsigned int)maxNumOfFields);

        map<string, string> mm;
        for (auto fv: fvs)
        {
            mm[fvField(fv)] = fvValue(fv);
        }

        for (int j = 0; j < maxNumOfFields; j ++)
        {
            EXPECT_EQ(mm[field(j)], value(j));
        }
    }

    /* Del operation and second set operation will be merged */

    /* Del operation */
    p.del(key);

    /* Second set operation */
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < maxNumOfFields * 2; j += 2)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set(key, fields);
    }

    /* Second pop operation */
    {
        int ret = cs.select(&selectcs);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), key);
        EXPECT_EQ(kfvOp(kco), "SET");

        auto fvs = kfvFieldsValues(kco);

        /* size of fvs should be maxNumOfFields, no "field 1" left from first set*/
        EXPECT_EQ(fvs.size(), (unsigned int)maxNumOfFields);

        map<string, string> mm;
        for (auto fv: fvs)
        {
            mm[fvField(fv)] = fvValue(fv);
        }

        for (int j = 0; j < maxNumOfFields * 2; j += 2)
        {
            EXPECT_EQ(mm[field(j)], value(j));
        }
    }

    /* Get data directly from table in redis DB*/
    Table t(&db, tableName);
    vector<FieldValueTuple> values;
    t.get(key, values);
    /* size of values should be maxNumOfFields, no "field 1" left from first set */
    EXPECT_EQ(values.size(), (unsigned int)maxNumOfFields);

    /*
     * Third pop operation, consumer received two consectuive signals.
     * data depleted upon first one
     */
    {
        int ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::OBJECT);
        KeyOpFieldsValuesTuple kco;
        c.pop(kco);
        EXPECT_EQ(kfvKey(kco), "");
    }

    /* Third select operation */
    {
        int ret = cs.select(&selectcs, 1000);
        EXPECT_EQ(ret, Select::TIMEOUT);
    }

    /* State Queue should be empty */
    RedisCommand keys;
    keys.format("KEYS %s*", (c.getStateHashPrefix() + tableName).c_str());
    RedisReply r(&db, keys, REDIS_REPLY_ARRAY);
    auto qlen = r.getContext()->elements;
    EXPECT_EQ(qlen, 0U);
}

TEST(ConsumerStateTable, singlethread)
{
    clearDB();

    int index = 0;
    string tableName = "UT_REDIS_THREAD_" + to_string(index);
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ProducerStateTable p(&db, tableName);

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

    ConsumerStateTable c(&db, tableName);
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

    EXPECT_LE(numberOfKeysSet, numberOfKeyDeleted);
    EXPECT_EQ(ret, Select::OBJECT);

    cout << "Done. Waiting for all job to finish " << NUMBER_OF_OPS << " jobs." << endl;

    /* State Queue should be empty */
    RedisCommand keys;
    keys.format("KEYS %s*", (c.getStateHashPrefix() + tableName).c_str());
    RedisReply r(&db, keys, REDIS_REPLY_ARRAY);
    auto qlen = r.getContext()->elements;
    EXPECT_EQ(qlen, 0U);

    cout << endl << "Done." << endl;
}

TEST(ConsumerStateTable, test)
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

TEST(ConsumerStateTable, multitable)
{
    DBConnector db(TEST_DB, "localhost", 6379, 0);
    ConsumerStateTable *consumers[NUMBER_OF_THREADS];
    vector<string> tablenames(NUMBER_OF_THREADS);
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
        tablenames[i] = string("UT_REDIS_THREAD_") + to_string(i);
        consumers[i] = new ConsumerStateTable(&db, tablenames[i]);
        producerThreads[i] = new thread(producerWorker, i);
    }

    for (i = 0; i < NUMBER_OF_THREADS; i++)
        cs.addSelectable(consumers[i]);

    while (1)
    {
        Selectable *is;

        ret = cs.select(&is);
        EXPECT_EQ(ret, Select::OBJECT);

        ((ConsumerStateTable *)is)->pop(kco);
        if (kfvOp(kco) == "SET")
        {
            numberOfKeysSet++;
            validateFields(kfvKey(kco), kfvFieldsValues(kco));
        } else if (kfvOp(kco) == "DEL")
        {
            numberOfKeyDeleted++;
            if ((numberOfKeyDeleted % 100) == 0)
                cout << "-" << flush;
        }

        if (numberOfKeyDeleted == NUMBER_OF_OPS * NUMBER_OF_THREADS)
            break;
    }

    EXPECT_LE(numberOfKeysSet, numberOfKeyDeleted);

    /* Making sure threads stops execution */
    for (i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i]->join();
        delete consumers[i];
        delete producerThreads[i];
    }


    /* State Queue should be empty */
    RedisCommand keys;
    keys.format("KEYS %s*", (consumers[0]->getStateHashPrefix() + tablenames[0]).c_str());
    RedisReply r(&db, keys, REDIS_REPLY_ARRAY);
    auto qlen = r.getContext()->elements;
    EXPECT_EQ(qlen, 0U);

    cout << endl << "Done." << endl;
}
