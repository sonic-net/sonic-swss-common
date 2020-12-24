#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "gtest/gtest.h"
#include <unistd.h>

using namespace std;
using namespace swss;

void setLoglevel(DBConnector& db, const string& key, const string& loglevel)
{
    ProducerStateTable table(&db, key);
    FieldValueTuple fv(DAEMON_LOGLEVEL, loglevel);
    std::vector<FieldValueTuple>fieldValues = { fv };
    table.set(key, fieldValues);
}

void clearLoglevelDB()
{
    DBConnector db("LOGLEVEL_DB", 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void prioNotify(const string &component, const string &prioStr)
{
    Logger::priorityStringMap.at(prioStr);
}

void checkLoglevel(DBConnector& db, const string& key, const string& loglevel)
{
    string redis_key = key + ":" + key;
    auto level = db.hget(redis_key, DAEMON_LOGLEVEL);
    EXPECT_FALSE(level == nullptr);
    if (level != nullptr)
    {
        EXPECT_EQ(*level, loglevel);
    }
}

TEST(LOGGER, loglevel)
{
    DBConnector db("LOGLEVEL_DB", 0);
    clearLoglevelDB();

    string key1 = "table1", key2 = "table2", key3 = "table3";

    cout << "Setting log level for table1." << endl;
    Logger::linkToDbNative(key1);

    sleep(1);

    cout << "Checking log level for table1." << endl;
    checkLoglevel(db, key1, "NOTICE");

    cout << "Setting log level for tables." << endl;
    Logger::linkToDb(key2, prioNotify, "NOTICE");
    Logger::linkToDb(key3, prioNotify, "INFO");
    setLoglevel(db, key1, "DEBUG");

    sleep(1);

    cout << "Checking log levels." << endl;
    checkLoglevel(db, key1, "DEBUG");
    checkLoglevel(db, key2, "NOTICE");
    checkLoglevel(db, key3, "INFO");

    cout << "Done." << endl;
}
