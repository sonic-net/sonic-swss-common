#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <deque>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/notificationconsumer.h"
#include "common/notificationproducer.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/table.h"
#include "common/zmqserver.h"
#include "common/zmqclient.h"
#include "common/zmqproducerstatetable.h"
#include "common/zmqconsumerstatetable.h"

using namespace std;
using namespace swss;

#define TEST_DB           "APPL_DB" // Need to test against a DB which uses a colon table name separator due to hardcoding in consumer_table_pops.lua
#define NUMBER_OF_THREADS    (5) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS        (100)
#define MAX_FIELDS       10 // Testing up to 30 fields objects
#define PRINT_SKIP           (10) // Print + for Producer and - for Consumer for every 100 ops
#define MAX_KEYS             (10)       // Testing up to 30 keys objects

static inline string field(int i)
{
    return string("field ") + to_string(i);
}

static inline string value(int i)
{
    if (i == 0) return string(); // empty
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
    EXPECT_TRUE((bool)is);
    return ret;
}

static bool allDataReceived = false;

static void producerWorker(string tableName, string endpoint, bool dbPersistence)
{
    DBConnector db(TEST_DB, 0, true);
    ZmqClient client(endpoint);
    ZmqProducerStateTable p(&db, tableName, client, dbPersistence);
    cout << "Producer thread started: " << tableName << endl;

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < MAX_FIELDS; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        p.set("set_key_" + to_string(i), fields);
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        p.del("del_key_" + to_string(i));
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        std::vector<KeyOpFieldsValuesTuple> vkco;
        vkco.resize(MAX_KEYS);
        for (int j = 0; j < MAX_KEYS; j++)
        {
            auto& kco = vkco[j];
            auto& values = kfvFieldsValues(kco);
            kfvKey(kco) = "batch_set_key_" + to_string(i) + "_" + to_string(j);
            kfvOp(kco) = "SET";
            
            for (int k = 0; k < MAX_FIELDS; k++)
            {
                FieldValueTuple t(field(k), value(k));
                values.push_back(t);
            }
        }

        p.set(vkco);
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        std::vector<std::string> keys;
        for (int j = 0; j < MAX_KEYS; j++)
        {
            keys.push_back("batch_del_key_" + to_string(i) + "_" + to_string(j));
        }

        p.del(keys);
    }

    // wait all data been received by consumer
    while (!allDataReceived)
    {
        sleep(1);
    }

    if (dbPersistence)
    {
        // wait all persist data write to redis
        while (p.dbUpdaterQueueSize() > 0)
        {
            sleep(1);
        }
    }

    cout << "Producer thread ended: " << tableName << endl;
}

// Reusing the same keys as the producerWorker so that the same consumer thread
// can be used.
static void producerBatchWorker(string tableName, string endpoint, bool dbPersistence)
{
    DBConnector db(TEST_DB, 0, true);
    ZmqClient client(endpoint);
    ZmqProducerStateTable p(&db, tableName, client, dbPersistence);
    cout << "Producer thread started: " << tableName << endl;
    std::vector<KeyOpFieldsValuesTuple> kcos;

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        vector<FieldValueTuple> fields;
        for (int j = 0; j < MAX_FIELDS; j++)
        {
            FieldValueTuple t(field(j), value(j));
            fields.push_back(t);
        }
        kcos.push_back(KeyOpFieldsValuesTuple("set_key_" + to_string(i), SET_COMMAND, fields));
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        kcos.push_back(KeyOpFieldsValuesTuple("del_key_" + to_string(i), DEL_COMMAND, vector<FieldValueTuple>{}));
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        for (int j = 0; j < MAX_KEYS; j++)
        {
            vector<FieldValueTuple> fields;
            for (int k = 0; k < MAX_FIELDS; k++)
            {
                FieldValueTuple t(field(k), value(k));
                fields.push_back(t);
            }
            kcos.push_back(KeyOpFieldsValuesTuple("batch_set_key_" + to_string(i) + "_" + to_string(j), SET_COMMAND, fields));
        }
    }

    for (int i = 0; i < NUMBER_OF_OPS; i++)
    {
        for (int j = 0; j < MAX_KEYS; j++)
        {
            kcos.push_back(KeyOpFieldsValuesTuple("batch_del_key_" + to_string(i) + "_" + to_string(j), DEL_COMMAND, vector<FieldValueTuple>{}));
        }
    }

    p.send(kcos);

    // wait all data been received by consumer
    while (!allDataReceived)
    {
        sleep(1);
    }

    if (dbPersistence)
    {
        // wait all persist data write to redis
        while (p.dbUpdaterQueueSize() > 0)
        {
            sleep(1);
        }
    }

    client.close();
    cout << "Producer thread ended: " << tableName << endl;
}

// variable used by consumer worker
static int setCount = 0;
static int delCount = 0;
static int batchSetCount = 0;
static int batchDelCount = 0;
    
static void consumerWorker(string tableName, string endpoint, bool dbPersistence)
{
    cout << "Consumer thread started: " << tableName << endl;
    
    DBConnector db(TEST_DB, 0, true);
    ZmqServer server(endpoint);
    ZmqConsumerStateTable c(&db, tableName, server, 128, 0, dbPersistence);
    Select cs;
    cs.addSelectable(&c);

    //validate received data
    Selectable *selectcs;
    std::deque<KeyOpFieldsValuesTuple> vkco;
    int ret = 0;

    while (setCount < NUMBER_OF_THREADS * NUMBER_OF_OPS
            || delCount < NUMBER_OF_THREADS * NUMBER_OF_OPS
            || batchSetCount < NUMBER_OF_THREADS * NUMBER_OF_OPS * MAX_KEYS
            || batchDelCount < NUMBER_OF_THREADS * NUMBER_OF_OPS * MAX_KEYS)
    {
        if ((ret = cs.select(&selectcs, 10, true)) == Select::OBJECT)
        {
            c.pops(vkco);
            while (!vkco.empty())
            {
                auto &kco = vkco.front();
                auto key = kfvKey(kco);
                auto op = kfvOp(kco);
                auto fvs = kfvFieldsValues(kco);

                if (key.rfind("batch_set_key_", 0) == 0)
                {
                    EXPECT_EQ(fvs.size(), MAX_FIELDS);
                    batchSetCount++;
                }
                else if (key.rfind("set_key_", 0) == 0)
                {
                    EXPECT_EQ(fvs.size(), MAX_FIELDS);
                    setCount++;
                }
                else if (key.rfind("batch_del_key_", 0) == 0)
                {
                    EXPECT_EQ(fvs.size(), 0);
                    batchDelCount++;
                }
                else if (key.rfind("del_key_", 0) == 0)
                {
                    EXPECT_EQ(fvs.size(), 0);
                    delCount++;
                }

                vkco.pop_front();
            }
        }
    }

    allDataReceived = true;

    if (dbPersistence)
    {
        // wait all persist data write to redis
        while (c.dbUpdaterQueueSize() > 0)
        {
            sleep(1);
        }
    }

    server.close();
    cout << "Consumer thread ended: " << tableName << endl;
}

static void testMethod(bool producerPersistence)
{
    std::string testTableName = "ZMQ_PROD_CONS_UT";
    std::string pushEndpoint = "tcp://localhost:1234";
    std::string pullEndpoint = "tcp://*:1234";
    thread *producerThreads[NUMBER_OF_THREADS];

    // reset receive data counter
    setCount = 0;
    delCount = 0;
    batchSetCount = 0;
    batchDelCount = 0;
    allDataReceived = false;

    // start consumer first, SHM can only have 1 consumer per table.
    thread *consumerThread = new thread(consumerWorker, testTableName, pullEndpoint, !producerPersistence);

    cout << "Starting " << NUMBER_OF_THREADS << " producers" << endl;
    /* Starting the producer before the producer */
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i] = new thread(producerWorker, testTableName, pushEndpoint, producerPersistence);
    }

    cout << "Done. Waiting for all job to finish " << NUMBER_OF_OPS << " jobs." << endl;
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i]->join();
        delete producerThreads[i];
    }

    consumerThread->join();
    delete consumerThread;

    EXPECT_EQ(setCount, NUMBER_OF_THREADS * NUMBER_OF_OPS);
    EXPECT_EQ(delCount, NUMBER_OF_THREADS * NUMBER_OF_OPS);
    EXPECT_EQ(batchSetCount, NUMBER_OF_THREADS * NUMBER_OF_OPS * MAX_KEYS);
    EXPECT_EQ(batchDelCount, NUMBER_OF_THREADS * NUMBER_OF_OPS * MAX_KEYS);

    // check presist data in redis
    DBConnector db(TEST_DB, 0, true);
    Table table(&db, testTableName);
    std::vector<std::string> keys;
    table.getKeys(keys);
    setCount = 0;
    batchSetCount = 0;
    for (string& key : keys)
    {
        if (key.rfind("batch_set_key_", 0) == 0)
        {
            batchSetCount++;
        }
        else if (key.rfind("set_key_", 0) == 0)
        {
            setCount++;
        }
    }
    EXPECT_EQ(setCount, NUMBER_OF_OPS);
    EXPECT_EQ(batchSetCount, NUMBER_OF_OPS * MAX_KEYS);

    cout << endl << "Done." << endl;
}

static void testBatchMethod(bool producerPersistence)
{
    std::string testTableName = "ZMQ_PROD_CONS_UT";
    std::string pushEndpoint = "tcp://localhost:1234";
    std::string pullEndpoint = "tcp://*:1234";
    thread *producerThreads[NUMBER_OF_THREADS];

    // reset receive data counter
    setCount = 0;
    delCount = 0;
    batchSetCount = 0;
    batchDelCount = 0;
    allDataReceived = false;

    // start consumer first, SHM can only have 1 consumer per table.
    thread *consumerThread = new thread(consumerWorker, testTableName, pullEndpoint, !producerPersistence);

    cout << "Starting " << NUMBER_OF_THREADS << " producers" << endl;
    /* Starting the producer before the producer */
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i] = new thread(producerBatchWorker, testTableName, pushEndpoint, producerPersistence);
    }

    cout << "Done. Waiting for all job to finish " << NUMBER_OF_OPS << " jobs." << endl;
    for (int i = 0; i < NUMBER_OF_THREADS; i++)
    {
        producerThreads[i]->join();
        delete producerThreads[i];
    }

    consumerThread->join();
    delete consumerThread;

    EXPECT_EQ(setCount, NUMBER_OF_THREADS * NUMBER_OF_OPS);
    EXPECT_EQ(delCount, NUMBER_OF_THREADS * NUMBER_OF_OPS);
    EXPECT_EQ(batchSetCount, NUMBER_OF_THREADS * NUMBER_OF_OPS * MAX_KEYS);
    EXPECT_EQ(batchDelCount, NUMBER_OF_THREADS * NUMBER_OF_OPS * MAX_KEYS);

    // check presist data in redis
    DBConnector db(TEST_DB, 0, true);
    Table table(&db, testTableName);
    std::vector<std::string> keys;
    table.getKeys(keys);
    setCount = 0;
    batchSetCount = 0;
    for (string& key : keys)
    {
        if (key.rfind("batch_set_key_", 0) == 0)
        {
            batchSetCount++;
        }
        else if (key.rfind("set_key_", 0) == 0)
        {
            setCount++;
        }
    }
    EXPECT_EQ(setCount, NUMBER_OF_OPS);
    EXPECT_EQ(batchSetCount, NUMBER_OF_OPS * MAX_KEYS);
    
    cout << endl << "Done." << endl;
}

TEST(ZmqConsumerStateTable, test)
{
    // test with persist by consumer
    testMethod(false);
}

TEST(ZmqProducerStateTable, test)
{
    // test with persist by producer
    testMethod(true);
}

TEST(ZmqConsumerStateTableBatch, test)
{
    // test with persist by consumer
    testBatchMethod(false);
}

TEST(ZmqProducerStateTableBatch, test)
{
    // test with persist by producer
    testBatchMethod(true);
}

TEST(ZmqConsumerStateTableBatchBufferOverflow, test)
{
    std::string testTableName = "ZMQ_PROD_CONS_UT";
    std::string pushEndpoint = "tcp://localhost:1234";

    DBConnector db(TEST_DB, 0, true);
    ZmqClient client(pushEndpoint);
    ZmqProducerStateTable p(&db, testTableName, client, true);

    // Send a large message and expect exception thrown.
    std::vector<KeyOpFieldsValuesTuple> kcos;
    for (int i = 0; i <= MQ_RESPONSE_MAX_COUNT; i++)
    {
        kcos.push_back(KeyOpFieldsValuesTuple("key", DEL_COMMAND, vector<FieldValueTuple>{}));
    }
    EXPECT_ANY_THROW(p.send(kcos));
}
