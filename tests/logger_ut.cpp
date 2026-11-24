#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "common/redisreply.h"
#include "logger_ut.h"
#include "gtest/gtest.h"
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <map>
#include <vector>
#include <mutex>
#include <future>
#include <chrono>

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

// Registration sets the component's defaults before it adds the observer, so
// a running setting thread never finds the observer without its defaults.
// Each default's lock is held in turn, so each set() is pinned before the
// observer insert.
TEST(LOGGER, DefaultsSetBeforeObserver)
{
    clearConfigDB();
    auto& logger = Logger::getInstance();

    const std::vector<std::pair<string, std::mutex*>> cases = {
        { "defaults_first_prio_component", &logger.m_defaultPrios.m_mutex },
        { "defaults_first_output_component", &logger.m_defaultOutputs.m_mutex },
    };
    for (const auto& c : cases)
    {
        SCOPED_TRACE(c.first);
        const string& key = c.first;

        std::unique_lock<std::mutex> lock(*c.second);
        auto registered = std::async(std::launch::async, [&key]() {
            Logger::linkToDbWithOutput(key, recordPrio, "INFO", recordOutput, "SYSLOG");
        });
        bool waited = registered.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout;
        bool observed = logger.m_settingChangeObservers.contains(key);
        lock.unlock();

        EXPECT_TRUE(waited) << "registration did not wait for the defaults lock";
        EXPECT_FALSE(observed) << "observer added before the component's defaults";
        ASSERT_EQ(registered.wait_for(std::chrono::seconds(10)), std::future_status::ready);
        registered.get();
        EXPECT_TRUE(waitNotified(key, "INFO", "SYSLOG"));
    }
}

static std::map<string, std::vector<string>> g_prioHistory;

static void recordPrioHistory(const string& component, const string& prio)
{
    std::lock_guard<std::mutex> lock(g_notifiedMutex);
    g_prioHistory[component].push_back(prio);
}

// The setting thread's start-up pass reverts only components whose row is
// gone; a component with a configured row keeps its level throughout.
TEST(LOGGER, ReconcileKeepsConfiguredRow)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    const string key = "reconcile_kept_component";
    const string sentinel = "reconcile_sentinel_component";
    setLoglevel(db, key, "ERROR");
    Logger::linkToDbWithOutput(key, recordPrioHistory, "INFO", recordOutput, "SYSLOG");
    Logger::linkToDbWithOutput(sentinel, recordPrio, "INFO", recordOutput, "SYSLOG");
    Logger::restartLogger();

    // Events are handled after the start-up pass, so once this change is
    // applied the pass is over.
    setLoglevel(db, sentinel, "DEBUG");
    ASSERT_TRUE(waitNotified(sentinel, "DEBUG", "SYSLOG"));

    // A setting thread from an earlier test may also apply the row's SET,
    // so ERROR can be notified twice; the default never is.
    {
        std::lock_guard<std::mutex> lock(g_notifiedMutex);
        const auto& history = g_prioHistory[key];
        EXPECT_FALSE(history.empty());
        for (const auto& prio : history)
        {
            EXPECT_EQ(prio, "ERROR");
        }
    }
    checkStateLoglevel(key, "ERROR", "SYSLOG");
}

// Registration holds the setting lock from its CONFIG_DB read to its STATE_DB
// publish, so it waits while the setting thread applies a change.
TEST(LOGGER, RegistrationTakesSettingLock)
{
    clearConfigDB();
    const string key = "lock_wait_component";

    std::unique_lock<std::mutex> lock(Logger::getInstance().m_settingMutex);
    auto registered = std::async(std::launch::async, [&key]() {
        Logger::linkToDbWithOutput(key, recordPrio, "INFO", recordOutput, "SYSLOG");
    });
    bool waited = registered.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout;
    lock.unlock();

    EXPECT_TRUE(waited) << "registration did not wait for the setting lock";
    ASSERT_EQ(registered.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    registered.get();
    EXPECT_TRUE(waitNotified(key, "INFO", "SYSLOG"));
    checkStateLoglevel(key, "INFO", "SYSLOG");
}

// The subscription replays only rows that exist, so a row deleted while no
// setting thread runs produces no DEL. The next setting thread's start-up
// pass reverts the component to its default.
TEST(LOGGER, ReconcileRevertsRowDeletedBeforeSubscribe)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    const string key = "gap_component";
    setLoglevel(db, key, "DEBUG");
    Logger::linkToDbWithOutput(key, recordPrio, "INFO", recordOutput, "SYSLOG");
    ASSERT_TRUE(waitNotified(key, "DEBUG", "SYSLOG"));

    Logger::getInstance().terminateSettingThread();
    swss::Table(&db, CFG_LOGGER_TABLE_NAME).del(key);
    Logger::restartLogger();

    EXPECT_TRUE(waitNotified(key, "INFO", "SYSLOG"));
    waitStateLoglevel(key, "INFO", "SYSLOG");
}

// A publish that fails after the STATE_DB table exists, as when the
// connection drops, drops the table, so the next publish reconnects.
TEST(LOGGER, PublishRecoversAfterConnectionLoss)
{
    clearConfigDB();
    auto& logger = Logger::getInstance();

    Logger::linkToDbWithOutput("conn_loss_warmup", recordPrio, "NOTICE", recordOutput, "SYSLOG");
    checkStateLoglevel("conn_loss_warmup", "NOTICE", "SYSLOG");

    long long id;
    {
        std::lock_guard<std::mutex> lock(logger.m_settingMutex);
        ASSERT_TRUE(logger.m_stateTable);
        RedisReply r(logger.m_stateTable->m_pipe->getDBConnector(), "CLIENT ID", REDIS_REPLY_INTEGER);
        id = r.getContext()->integer;
    }
    DBConnector stateDb("STATE_DB", 0);
    RedisReply killed(&stateDb, "CLIENT KILL ID " + std::to_string(id), REDIS_REPLY_INTEGER);
    ASSERT_EQ(killed.getContext()->integer, 1);

    // This publish hits the dropped connection and fails. On a Unix socket
    // the write raises SIGPIPE, ignored here so it fails with EPIPE instead.
    auto savedPipe = signal(SIGPIPE, SIG_IGN);
    Logger::linkToDbWithOutput("conn_loss_failed", recordPrio, "INFO", recordOutput, "SYSLOG");
    EXPECT_FALSE(stateDb.exists(string(STATE_LOGGER_TABLE_NAME) + "|conn_loss_failed"));

    // The next one reconnects.
    Logger::linkToDbWithOutput("conn_loss_recovered", recordPrio, "ERROR", recordOutput, "SYSLOG");
    signal(SIGPIPE, savedPipe);
    checkStateLoglevel("conn_loss_recovered", "ERROR", "SYSLOG");
}

// Child side of PublishWithoutStateDb: exits 0 when registration returns
// without STATE_DB and the next setting change, with STATE_DB back, publishes.
[[ noreturn ]] static void publishWithoutStateDb(const char* deadPath)
{
    SonicDBConfig::reset();
    SonicDBConfig::initialize(deadPath);

    const string key = "state_down_component";
    Logger::linkToDbWithOutput(key, recordPrio, "INFO", recordOutput, "SYSLOG");
    if (!waitNotified(key, "INFO", "SYSLOG") || Logger::getInstance().m_stateTable)
    {
        _exit(1);
    }

    SonicDBConfig::reset();
    SonicDBConfig::initialize("./tests/redis_multi_db_ut_config/database_config.json");
    DBConnector stateDb("STATE_DB", 0);
    const string redisKey = string(STATE_LOGGER_TABLE_NAME) + "|" + key;
    if (stateDb.exists(redisKey))
    {
        _exit(2);
    }

    Logger::restartLogger();
    DBConnector db("CONFIG_DB", 0);
    setLoglevel(db, key, "DEBUG");
    for (int i = 0; i < 50; i++)
    {
        auto level = stateDb.hget(redisKey, DAEMON_LOGLEVEL);
        if (level && *level == "DEBUG")
        {
            _exit(0);
        }
        usleep(100 * 1000);
    }
    _exit(3);
}

// A STATE_DB failure at registration does not escape linkToDbWithOutput, and
// the component is published on its next setting change. Runs in a
// re-executed child, which points STATE_DB at a dead socket.
TEST(LOGGER, PublishWithoutStateDb)
{
    clearConfigDB();

    // The child runs this test again up to EXPECT_EXIT, so the name is fixed.
    const char* path = "/tmp/swss_logger_ut_state_db_down.json";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    ASSERT_GE(fd, 0);
    const string config = R"({
    "INSTANCES": {
        "redis": { "hostname": "127.0.0.1", "port": 6379, "unix_socket_path": "/var/run/redis/redis.sock" },
        "dead": { "hostname": "127.0.0.1", "port": 1, "unix_socket_path": "/nonexistent/redis.sock" }
    },
    "DATABASES": {
        "CONFIG_DB": { "id": 4, "separator": "|", "instance": "redis" },
        "STATE_DB": { "id": 6, "separator": "|", "instance": "dead" }
    },
    "VERSION": "1.0"
})";
    ASSERT_EQ(write(fd, config.data(), config.size()), static_cast<ssize_t>(config.size()));
    close(fd);

    const string savedStyle = testing::FLAGS_gtest_death_test_style;
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    EXPECT_EXIT(publishWithoutStateDb(path), testing::ExitedWithCode(0), "");
    testing::FLAGS_gtest_death_test_style = savedStyle;

    unlink(path);
}
