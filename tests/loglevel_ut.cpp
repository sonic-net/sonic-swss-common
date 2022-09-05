#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "common/loglevel.h"
#include "gtest/gtest.h"
#include <unistd.h>
using namespace std;
using namespace swss;

void setLoglevel2(DBConnector& db, const string& key, const string& loglevel)
{
    swss::Table table(&db, key);
    FieldValueTuple fv(DAEMON_LOGLEVEL, loglevel);
    std::vector<FieldValueTuple>fieldValues = { fv };
    table.set(key, fieldValues);
}

void clearConfigDB2()
{
    DBConnector db("CONFIG_DB", 0);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

void checkLoglevel2(DBConnector& db, const string& key, const string& loglevel)
{
    string redis_key = "LOGGER|" + key;
    auto level = db.hget(redis_key, DAEMON_LOGLEVEL);
    EXPECT_FALSE(level == nullptr);
    if (level != nullptr)
    {
        EXPECT_EQ(*level, loglevel);
    }
}

TEST(LOGLEVEL, loglevel)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB2();

    string key1 = "table1", key2 = "table2", key3 = "table3";

    cout << "Setting log level NOTICE for table1." << endl;
    setLoglevel2(db, key1, "NOTICE");
    cout << "Setting log level DEBUG for table1." << endl;
    setLoglevel2(db, key1, "DEBUG");
    cout << "Setting log level SAI_LOG_LEVEL_ERROR for table1." << endl;
    setLoglevel2(db, key1, "SAI_LOG_LEVEL_ERROR");

    sleep(1);
    char* argv[1] = {"-d"};
    swssloglevel(1,argv);
    sleep(1);
    cout << "Checking log level for tables." << endl;
    checkLoglevel2(db, key1, "NOTICE");
    checkLoglevel2(db, key2, "NOTICE");
    checkLoglevel2(db, key3, "SAI_LOG_LEVEL_NOTICE");

    cout << "Done." << endl;
}
