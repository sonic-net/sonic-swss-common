#include "common/dbconnector.h"
#include "common/producertable.h"
#include "common/consumertable.h"
#include "common/select.h"
#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include "gtest/gtest.h"

using namespace std;
using namespace swss;

#define TEST_VIEW            (7)
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
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
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

void clearDB()
{
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

TEST(DBConnector, test)
{
    std::thread *producerThreads, *consumerThreads;
    producerThreads = new std::thread[NUMBER_OF_THREADS];
    consumerThreads = new std::thread[NUMBER_OF_THREADS];

    clearDB();

    cout << "Starting " << NUMBER_OF_THREADS*2 << " producers and consumers on redis" << endl;
    /* Starting the consumer before the producer */
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        consumerThreads[i] = std::thread(consumerWorker, i);
        producerThreads[i] = std::thread(producerWorker, i);
    }

    cout << "Done. Waiting for all job to finish " << NUMBER_OF_OPS << " jobs." << endl;

    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i].join();
        consumerThreads[i].join();
    }
}

TEST(DBConnector, multitable)
{
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    ConsumerTable *consumers[NUMBER_OF_THREADS];
    thread producerThreads[NUMBER_OF_THREADS];
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
        producerThreads[i] = thread(producerWorker, i);
    }

    for (i = 0; i < NUMBER_OF_THREADS; i++)
        cs.addSelectable(consumers[i]);

    while (1)
    {
        Selectable *i;
        int fd;

        ret = cs.select(&i, &fd);
        EXPECT_EQ(ret, Select::OBJECT);

        ((ConsumerTable *)i)->pop(kco);
        if (kfvOp(kco) == "SET")
        {
            numberOfKeysSet++;
            validateFields(kfvKey(kco), kfvFieldsValues(kco));
        } else
        {
            numberOfKeyDeleted++;
        }

        if ((numberOfKeysSet == NUMBER_OF_OPS * NUMBER_OF_THREADS) &&
            (numberOfKeyDeleted == NUMBER_OF_OPS * NUMBER_OF_THREADS))
            break;
    }

    /* Making sure threads stops execution */
    for (i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i].join();
        delete consumers[i];
    }

    cout << endl << "Done." << endl;
}

