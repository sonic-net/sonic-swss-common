#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <future>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/select.h"
#include "common/table.h"
#include "common/pubsub.h"

using namespace std;
using namespace swss;

static const int NUMBER_OF_OPS     = 40;
static const int MAX_FIELDS_DIV    = 30; // Testing up to 30 fields objects

static const string testTableName = "UT_REDIS_TABLE";
static const string testTableName2 = "UT_REDIS_TABLE2";
static const string testTableName3 = "UT_REDIS_TABLE3";
static const string testTableName4 = "UT_REDIS_TABLE4";

static inline int getMaxFields(int i)
{
    return (i/MAX_FIELDS_DIV) + 1;
}

static inline string key(int index, int keyid)
{
    return string("key_") + to_string(index) + ":" + to_string(keyid);
}

static inline string field(int index, int keyid)
{
    return string("field ") + to_string(index) + ":" + to_string(keyid);
}

static inline string value(int index, int keyid)
{
    return string("value ") + to_string(index) + ":" + to_string(keyid);
}

static inline void clearDB()
{
    DBConnector db("TEST_DB", 0, true);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

namespace pubsub_ut
{
    using namespace swss;
    using namespace std;

    class PubSubTestFixture : public ::testing::Test
    {
    public:
        vector<shared_ptr<thread>> listenerPool;
        vector<shared_ptr<thread>> producerPool;
        vector<int> totalEvents;
        std::atomic<bool> stopSubscriber;
        vector<string> testTables;
        shared_ptr<swss::DBConnector> m_test_db;

        PubSubTestFixture()
        {
            m_test_db = make_shared<swss::DBConnector>("TEST_DB", 0, true);
        }

        virtual void SetUp() override
        {
            totalEvents.clear();
            stopSubscriber = false;
            testTables.clear();
        }

        std::string getkeySpace(std::string table)
        {
            return "__keyspace@" + to_string(m_test_db->getDbId()) + "__:" + table + ":" + "*";
        }

        void subscriberWorker(int index)
        {
            std::string clientName = "test_pubsub" + to_string(index);
            PubSub c(m_test_db.get());
            vector<string> temp;
            for (auto table : testTables)
            {
                temp.push_back(getkeySpace(table)); /* Subscribe to multiple channels */
            }
            if (temp.size() > 1) {
                for (auto pattern : temp) {
                    c.psubscribe(pattern);
                }
            } else {
                c.psubscribe(getkeySpace(testTables[0]));
            }
            totalEvents[index] = select_loop(&c);
        }

        int select_loop(PubSub* s)
        {
            int num_events = 0;
            while(true && !stopSubscriber)
            {
                try {
                    // Use PubSub's get_message method with timeout
                    auto message = s->get_message(1.0, false); // 1 second timeout
                    if (!message.empty()) {
                        num_events++;
                    }
                } catch (const std::exception& e) {
                    // Timeout or other exception - continue
                    continue;
                }
            }
            return num_events;
        }

        void producerWorker(int index, const std::string& table)
        {
            Table p(m_test_db.get(), table);

            for (int i = 0; i < NUMBER_OF_OPS; i++)
            {
                vector<FieldValueTuple> fields;
                int maxNumOfFields = getMaxFields(i);

                for (int j = 0; j < maxNumOfFields; j++)
                {
                    FieldValueTuple t(field(index, j), value(index, j));
                    fields.push_back(t);
                }

                if ((i % 100) == 0)
                {
                    cout << "+" << flush;
                }

                p.set(key(index, i), fields);
            }

            for (int i = 0; i < NUMBER_OF_OPS; i++)
            {
                p.del(key(index, i));
            }
        }

        vector<string> getTableParam(int num_tables)
        {
            vector<string> ret;
            if (num_tables == 1) ret.push_back(testTableName);
            else if (num_tables == 2) ret = vector<string> ({testTableName, testTableName2});
            else ret = vector<string>{testTableName, testTableName2, testTableName3, testTableName4}; // max 4
            return ret;
        }

        void start_stop_producers(vector<string> tables)
        {
            sleep(1); // buffer for all subscribers to reach selectable state
            producerPool.clear();
            cout << "Producer Threads Started" << endl;

            int index = 0;
            std::for_each(tables.begin(), tables.end(), [&](const string& table) {
                producerPool.push_back(std::make_shared<thread>(&PubSubTestFixture::producerWorker, this, index, table));
                index++;
            });

            // Join all producer threads
            std::for_each(producerPool.begin(), producerPool.end(), [](auto& th) {th->join();});
            cout << "Producer Threads Joined" << endl;
            sleep(1); // time buffer until all the events are read by subscriber
        }
    };

    TEST_F(PubSubTestFixture, testUnSubscribe)
    {
        PubSub key_space_sub(m_test_db.get());
        auto tables = getTableParam(2);

        for (auto table : tables)
        {
            key_space_sub.psubscribe(getkeySpace(table));
        }

        auto future = std::async(&PubSubTestFixture::select_loop, this, &key_space_sub);
        start_stop_producers(tables);
        stopSubscriber = true; // Exit from selectable state

        int events = future.get();
        cout << "Num Events with two Subscriber: " << events << endl;
        cout << "Expected Events: " << NUMBER_OF_OPS * (int)tables.size() * 2 << endl;
        ASSERT_TRUE(events == NUMBER_OF_OPS * (int)tables.size() * 2);

        /* unsubscribe from one table */
        key_space_sub.punsubscribe(getkeySpace(tables[0]));

        stopSubscriber = false;
        future = std::async(&PubSubTestFixture::select_loop, this, &key_space_sub);
        start_stop_producers(tables);
        stopSubscriber = true;

        events = future.get();
        cout << "Num Events with one Subscriber: " << events << endl;
        ASSERT_TRUE(events == NUMBER_OF_OPS * 1 * 2); // Only one table is used

        stopSubscriber = false;
        /* unsubscribe from all the tables */
        key_space_sub.punsubscribe(getkeySpace(tables[1]));
        future = std::async(&PubSubTestFixture::select_loop, this, &key_space_sub);
        start_stop_producers(tables);
        stopSubscriber = true;

        events = future.get();
        ASSERT_TRUE(events == 0); // No events read
    }

    class PubSubTestFixtureParameterized :
        public PubSubTestFixture,
        public ::testing::WithParamInterface<std::tuple<int, int>>
    {
    public:
        PubSubTestFixtureParameterized() = default;
    };

    TEST_P(PubSubTestFixtureParameterized, testSubscription)
    {
        int numSubscribers = get<0>(GetParam());
        testTables = getTableParam(get<1>(GetParam()));

        cout << "Listener Threads Started" << endl;
        totalEvents.resize(numSubscribers, 0);

        for (auto i = 0; i < numSubscribers; i++)
        {
            listenerPool.push_back(std::make_shared<thread>(&PubSubTestFixture::subscriberWorker, this, i));
        }

        start_stop_producers(testTables);

        stopSubscriber = true; // Exit from selectable state

        std::for_each(listenerPool.begin(), listenerPool.end(), [](auto& th) {th->join();});
        cout << "Listener Threads Joined" << endl;

        int th_num = 0;
        std::for_each(totalEvents.begin(), totalEvents.end(), [&](auto& events) {
            cout << "Events Capture Thread " << th_num << " : " <<  events << endl;
            ASSERT_TRUE(events == NUMBER_OF_OPS * (int)testTables.size() * 2);
            th_num++;
        });
    }

    INSTANTIATE_TEST_SUITE_P(
        PubSubTests,
        PubSubTestFixtureParameterized,
        ::testing::Values(
                make_tuple(1, 1),
                make_tuple(1, 2),
                make_tuple(1, 4),
                make_tuple(2, 4),
                make_tuple(2, 2),
                make_tuple(4, 4)));
}
