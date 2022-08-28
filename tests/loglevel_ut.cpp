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
    swss::Table table(&db, key);
    FieldValueTuple fv(DAEMON_LOGLEVEL, loglevel);
    std::vector<FieldValueTuple>fieldValues = { fv };
    table.set(key, fieldValues);
}

void clearConfigDB()
{
    DBConnector db("CONFIG_DB", 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void checkLoglevel(DBConnector& db, const string& key, const string& loglevel)
{
    string redis_key = "LOGGER|" + key;
    auto level = db.hget(redis_key, DAEMON_LOGLEVEL);
    EXPECT_FALSE(level == nullptr);
    if (level != nullptr)
    {
        EXPECT_EQ(*level, loglevel);
    }
}

TEST(LOGGER, loglevel)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    string key1 = "table1", key2 = "table2", key3 = "table3";

    cout << "Setting log level NOTICE for table1." << endl;
    setLoglevel(db, key1, "NOTICE");
    cout << "Setting log level DEBUG for table1." << endl;
    setLoglevel(db, key1, "DEBUG");
    cout << "Setting log level SAI_LOG_LEVEL_ERROR for table1." << endl;
    setLoglevel(db, key1, "SAI_LOG_LEVEL_ERROR");

    sleep(1);
    swssloglevel("-d");
    sleep(1);
    cout << "Checking log level for tables." << endl;
    checkLoglevel(db, key1, "NOTICE");
    checkLoglevel(db, key2, "NOTICE");
    checkLoglevel(db, key3, "SAI_LOG_LEVEL_NOTICE");

    cout << "Done." << endl;
}
