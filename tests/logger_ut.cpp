#include "common/dbconnector.h"
#include "common/redisclient.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "gtest/gtest.h"
#include <unistd.h>

using namespace std;
using namespace swss;

void setLoglevel(DBConnector& db, const string& component, const string& loglevel)
{
    ProducerStateTable table(&db, component);
    FieldValueTuple fv(DAEMON_LOGLEVEL, loglevel);
    std::vector<FieldValueTuple>fieldValues = { fv };
    table.set(component, fieldValues);
}

void clearDB()
{
    DBConnector db("LOGLEVEL_DB", 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void prioNotify(const string &component, const string &prioStr)
{
    Logger::priorityStringMap.at(prioStr);
}

TEST(LOGGER, loglevel)
{
    DBConnector db("LOGLEVEL_DB", 0);
    RedisClient redisClient(&db);
    clearDB();

    string key1 = "table1", key2 = "table2", key3 = "table3";
    string redis_key1 = key1 + ":" + key1;
    string redis_key2 = key2 + ":" + key2;
    string redis_key3 = key3 + ":" + key3;

    cout << "Setting log level for table1." << endl;
    Logger::linkToDbNative(key1);

    sleep(1);

    cout << "Getting log level for table1." << endl;
    string level = *redisClient.hget(redis_key1, DAEMON_LOGLEVEL);
    EXPECT_EQ(level, "NOTICE");

    cout << "Setting log level for other tables." << endl;
    Logger::linkToDb(key2, prioNotify, "NOTICE");
    Logger::linkToDb(key3, prioNotify, "INFO");
    setLoglevel(db, key1, "DEBUG");

    sleep(1);

    cout << "Getting log levels." << endl;
    string level1 = *redisClient.hget(redis_key1, DAEMON_LOGLEVEL);
    string level2 = *redisClient.hget(redis_key2, DAEMON_LOGLEVEL);
    string level3 = *redisClient.hget(redis_key3, DAEMON_LOGLEVEL);

    EXPECT_EQ(level1, "DEBUG");
    EXPECT_EQ(level2, "NOTICE");
    EXPECT_EQ(level3, "INFO");

    cout << "Done." << endl;
}
