#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "logger_ut.h"
#include "gtest/gtest.h"
#include <unistd.h>

using namespace std;
using namespace swss;

void setLoglevel(DBConnector& db, const string& key, const string& loglevel)
{
    swss::Table table(&db, CFG_LOGGER_TABLE_NAME);
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

void prioNotify(const string &component, const string &prioStr)
{
    Logger::priorityStringMap.at(prioStr);
}

void checkLoglevel(DBConnector& db, const string& key, const string& loglevel)
{
    std::string key_prefix(CFG_LOGGER_TABLE_NAME);
    key_prefix+="|";
    string redis_key = key_prefix + key;
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

    // the native logger, table1, takes DEBUG from CONFIG_DB in its setting thread
    for (int i = 0; i < 50 && !Logger::isPrioEnabled(Logger::SWSS_DEBUG); i++)
    {
        usleep(100 * 1000);
    }
    EXPECT_TRUE(Logger::isPrioEnabled(Logger::SWSS_DEBUG));

    cout << "Done." << endl;
}

TEST(LOGGER, MacrosEvaluateArgumentsOnlyWhenEnabled)
{
    auto prio = Logger::getMinPrio();
    int calls = 0;
    auto arg = [&calls]() { calls++; return "arg"; };

    Logger::setMinPrio(Logger::SWSS_NOTICE);
    EXPECT_TRUE(Logger::isPrioEnabled(Logger::SWSS_NOTICE));
    EXPECT_FALSE(Logger::isPrioEnabled(Logger::SWSS_INFO));

    SWSS_LOG_DEBUG("%s", arg());
    SWSS_LOG_INFO("%s", arg());
    EXPECT_EQ(calls, 0);

    SWSS_LOG_NOTICE("%s", arg());
    SWSS_LOG_NOTICE("no arguments");
    EXPECT_EQ(calls, 1);

    Logger::setMinPrio(Logger::SWSS_DEBUG);
    EXPECT_TRUE(Logger::isPrioEnabled(Logger::SWSS_DEBUG));
    SWSS_LOG_DEBUG("%s", arg());
    EXPECT_EQ(calls, 2);

    Logger::setMinPrio(prio);
}

TEST(LOGGER, ScopeLoggersWriteOnlyAtDebug)
{
    auto prio = Logger::getMinPrio();
    Logger::swssOutputNotify("logger_ut", "STDOUT");

    Logger::setMinPrio(Logger::SWSS_NOTICE);
    testing::internal::CaptureStdout();
    {
        SWSS_LOG_ENTER();
        Logger::ScopeLogger scope(__LINE__, "outOfLine");
    }
    EXPECT_EQ(testing::internal::GetCapturedStdout(), "");

    Logger::setMinPrio(Logger::SWSS_DEBUG);
    testing::internal::CaptureStdout();
    {
        SWSS_LOG_ENTER();
        Logger::ScopeLogger scope(__LINE__, "outOfLine");
    }
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find(":> TestBody: enter"), string::npos);
    EXPECT_NE(output.find(":< TestBody: exit"), string::npos);
    EXPECT_NE(output.find(":> outOfLine: enter"), string::npos);
    EXPECT_NE(output.find(":< outOfLine: exit"), string::npos);

    Logger::swssOutputNotify("logger_ut", "SYSLOG");
    Logger::setMinPrio(prio);
}

TEST(LOGGER, CustomLogObserver)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    string key1 = "table1";
    Logger::linkToDb(key1, prioNotify, "NOTICE");
    Logger::restartLogger();

    sleep(1);

    cout << "Checking log level for table1." << endl;
    checkLoglevel(db, key1, "NOTICE");

    cout << "Setting log level for table1." << endl;
    setLoglevel(db, key1, "DEBUG");

    sleep(1);

    cout << "Checking log level for table1." << endl;
    checkLoglevel(db, key1, "DEBUG");
}
