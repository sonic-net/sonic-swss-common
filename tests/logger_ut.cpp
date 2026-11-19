#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "common/redisreply.h"
#include "logger_ut.h"
#include "gtest/gtest.h"
#include <unistd.h>
#include <map>
#include <mutex>

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

void checkStateLoglevel(const string& key, const string& loglevel, const string& output)
{
    DBConnector stateDb("STATE_DB", 0);
    string redis_key = string(STATE_LOGGER_TABLE_NAME) + "|" + key;
    auto level = stateDb.hget(redis_key, DAEMON_LOGLEVEL);
    auto out = stateDb.hget(redis_key, DAEMON_LOGOUTPUT);
    ASSERT_TRUE(level != nullptr);
    ASSERT_TRUE(out != nullptr);
    EXPECT_EQ(*level, loglevel);
    EXPECT_EQ(*out, output);
}

// The STATE_DB row is published after the observers run, so poll for it.
void waitStateLoglevel(const string& key, const string& loglevel, const string& output)
{
    DBConnector stateDb("STATE_DB", 0);
    string redis_key = string(STATE_LOGGER_TABLE_NAME) + "|" + key;
    for (int i = 0; i < 50; i++)
    {
        auto level = stateDb.hget(redis_key, DAEMON_LOGLEVEL);
        auto out = stateDb.hget(redis_key, DAEMON_LOGOUTPUT);
        if (level && out && *level == loglevel && *out == output)
        {
            return;
        }
        usleep(100 * 1000);
    }
    checkStateLoglevel(key, loglevel, output);
}

bool configRowExists(DBConnector& db, const string& key)
{
    return db.exists(string(CFG_LOGGER_TABLE_NAME) + "|" + key);
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
    EXPECT_FALSE(configRowExists(db, key1));
    checkStateLoglevel(key1, "NOTICE", "SYSLOG");

    cout << "Setting log level for tables." << endl;
    Logger::linkToDb(key2, prioNotify, "NOTICE");
    Logger::linkToDb(key3, prioNotify, "INFO");
    setLoglevel(db, key1, "DEBUG");

    sleep(1);

    cout << "Checking log levels." << endl;
    checkLoglevel(db, key1, "DEBUG");
    waitStateLoglevel(key1, "DEBUG", "SYSLOG");
    EXPECT_FALSE(configRowExists(db, key2));
    EXPECT_FALSE(configRowExists(db, key3));
    checkStateLoglevel(key2, "NOTICE", "SYSLOG");
    checkStateLoglevel(key3, "INFO", "SYSLOG");

    cout << "Done." << endl;
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
    EXPECT_FALSE(configRowExists(db, key1));
    checkStateLoglevel(key1, "NOTICE", "SYSLOG");

    cout << "Setting log level for table1." << endl;
    setLoglevel(db, key1, "DEBUG");

    sleep(1);

    cout << "Checking log level for table1." << endl;
    checkLoglevel(db, key1, "DEBUG");
}

static std::map<string, string> g_notifiedPrios;
static std::map<string, string> g_notifiedOutputs;
static std::mutex g_notifiedMutex;

static void recordPrio(const string& component, const string& prio)
{
    std::lock_guard<std::mutex> lock(g_notifiedMutex);
    g_notifiedPrios[component] = prio;
}

static void recordOutput(const string& component, const string& output)
{
    std::lock_guard<std::mutex> lock(g_notifiedMutex);
    g_notifiedOutputs[component] = output;
}

static bool waitNotified(const string& component, const string& prio, const string& output)
{
    for (int i = 0; i < 50; i++)
    {
        {
            std::lock_guard<std::mutex> lock(g_notifiedMutex);
            if (g_notifiedPrios[component] == prio && g_notifiedOutputs[component] == output)
            {
                return true;
            }
        }
        usleep(100 * 1000);
    }
    return false;
}

// An absent row is the default: registering writes nothing to CONFIG_DB, a
// configured row applies, and removing a field or the row reverts to the
// component's default.
TEST(LOGGER, AbsentRowIsDefault)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    string key = "absent_row_component";
    Logger::linkToDbWithOutput(key, recordPrio, "INFO", recordOutput, "SYSLOG");
    Logger::restartLogger();

    EXPECT_TRUE(waitNotified(key, "INFO", "SYSLOG"));
    EXPECT_FALSE(configRowExists(db, key));
    waitStateLoglevel(key, "INFO", "SYSLOG");

    swss::Table table(&db, CFG_LOGGER_TABLE_NAME);
    table.set(key, { { DAEMON_LOGLEVEL, "DEBUG" }, { DAEMON_LOGOUTPUT, "STDERR" } });
    EXPECT_TRUE(waitNotified(key, "DEBUG", "STDERR"));
    waitStateLoglevel(key, "DEBUG", "STDERR");

    table.hdel(key, DAEMON_LOGOUTPUT);
    EXPECT_TRUE(waitNotified(key, "DEBUG", "SYSLOG"));
    waitStateLoglevel(key, "DEBUG", "SYSLOG");

    table.del(key);
    EXPECT_TRUE(waitNotified(key, "INFO", "SYSLOG"));
    waitStateLoglevel(key, "INFO", "SYSLOG");
    EXPECT_FALSE(configRowExists(db, key));
}

// A row configured before registration applies at registration and is left
// as the operator wrote it.
TEST(LOGGER, ConfiguredRowAppliesAtRegistration)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    string key = "configured_row_component";
    setLoglevel(db, key, "ERROR");

    Logger::linkToDbWithOutput(key, recordPrio, "NOTICE", recordOutput, "SYSLOG");

    EXPECT_TRUE(waitNotified(key, "ERROR", "SYSLOG"));
    checkStateLoglevel(key, "ERROR", "SYSLOG");

    swss::Table table(&db, CFG_LOGGER_TABLE_NAME);
    std::vector<FieldValueTuple> fvs;
    ASSERT_TRUE(table.get(key, fvs));
    ASSERT_EQ(fvs.size(), 1u);
    EXPECT_EQ(fvField(fvs[0]), DAEMON_LOGLEVEL);
    EXPECT_EQ(fvValue(fvs[0]), "ERROR");
}

static long long connectionsReceived()
{
    DBConnector db("STATE_DB", 0);
    RedisReply r(&db, "INFO stats", REDIS_REPLY_STRING);
    string info = r.getReply<string>();
    const string field = "total_connections_received:";
    return std::stoll(info.substr(info.find(field) + field.size()));
}

// Publishing reuses one STATE_DB connection instead of opening one per
// component; registration itself still opens its CONFIG_DB connections.
TEST(LOGGER, PublishReusesStateDbConnection)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    const int count = 20;
    Logger::linkToDbWithOutput("reuse_component_warmup", recordPrio, "NOTICE", recordOutput, "SYSLOG");
    long long before = connectionsReceived();
    for (int i = 0; i < count; i++)
    {
        Logger::linkToDbWithOutput("reuse_component_" + std::to_string(i), recordPrio, "NOTICE", recordOutput, "SYSLOG");
    }
    long long opened = connectionsReceived() - before - 1;

    // Registration opens two CONFIG_DB connections; one more per publish
    // pair would make this 4 * count.
    EXPECT_LE(opened, 2 * count + 2);
    for (int i = 0; i < count; i++)
    {
        checkStateLoglevel("reuse_component_" + std::to_string(i), "NOTICE", "SYSLOG");
    }
}

// Child side of ForkedChildPublishesOnItsOwnConnection.
// Registration opens two CONFIG_DB connections; a STATE_DB table of its own
// opens two more, where reusing the parent's socket would open none.
[[ noreturn ]] static void registerInChild()
{
    long long before = connectionsReceived();
    Logger::linkToDbWithOutput("fork_child_component", recordPrio, "INFO", recordOutput, "SYSLOG");
    long long opened = connectionsReceived() - before - 1;

    DBConnector stateDb("STATE_DB", 0);
    auto level = stateDb.hget(string(STATE_LOGGER_TABLE_NAME) + "|fork_child_component", DAEMON_LOGLEVEL);
    _exit(opened >= 4 && level && *level == "INFO" ? 0 : 1);
}

// A forked child publishes on a connection of its own, and the parent's
// connection keeps working.
TEST(LOGGER, ForkedChildPublishesOnItsOwnConnection)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    Logger::linkToDbWithOutput("fork_parent_component", recordPrio, "NOTICE", recordOutput, "SYSLOG");

    // An earlier test may leave the level at DEBUG, where the setting thread
    // calls syslog() every second. A fork while it holds glibc's syslog lock
    // would hang the child, so no thread logs while this test forks.
    auto savedPrio = Logger::getMinPrio();
    Logger::setMinPrio(Logger::SWSS_NOTICE);
    usleep(10 * 1000); // let a syslog() call already under way finish
    EXPECT_EXIT(registerInChild(), testing::ExitedWithCode(0), "");
    Logger::setMinPrio(savedPrio);

    Logger::linkToDbWithOutput("fork_parent_component2", recordPrio, "ERROR", recordOutput, "SYSLOG");
    checkStateLoglevel("fork_parent_component2", "ERROR", "SYSLOG");
}
