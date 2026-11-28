#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "common/loglevel.h"
#include "logger_ut.h"
#include "gtest/gtest.h"
#include <unistd.h>
#include <fcntl.h>
using namespace swss;

TEST(LOGLEVEL, loglevel)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    std::string key1 = "table1", key2 = "table2", key3 = "SAI_API_table3";

    std::cout << "Setting log level NOTICE for table1." << std::endl;
    setLoglevel(db, key1, "NOTICE");
    std::cout << "Setting log level DEBUG for table1." << std::endl;
    setLoglevel(db, key2, "DEBUG");
    std::cout << "Setting log level SAI_LOG_LEVEL_ERROR for table1." << std::endl;
    setLoglevel(db, key3, "SAI_LOG_LEVEL_ERROR");

    sleep(1);

    char* argv[] = {"loglevel", "-d"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    optind = 0;
    swssloglevel(argc, argv);
    sleep(1);

    std::cout << "Checking log level for tables." << std::endl;
    checkLoglevel(db, key1, "NOTICE");
    checkLoglevel(db, key2, "NOTICE");
    checkLoglevel(db, key3, "SAI_LOG_LEVEL_NOTICE");

    std::cout << "Done." << std::endl;
}

// A component registered only in STATE_DB, with no CONFIG_DB row, is a valid
// target for -c and is listed by -p; -d resets configured rows only and
// creates none.
TEST(LOGLEVEL, RegisteredComponentWithoutConfigRow)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    DBConnector stateDb("STATE_DB", 0);
    swss::Table stateTable(&stateDb, STATE_LOGGER_TABLE_NAME);
    stateTable.set("registered1", { { DAEMON_LOGLEVEL, "NOTICE" }, { DAEMON_LOGOUTPUT, "SYSLOG" } });
    stateTable.set("registered2", { { DAEMON_LOGLEVEL, "NOTICE" }, { DAEMON_LOGOUTPUT, "SYSLOG" } });
    stateTable.set("SAI_API_registered3", { { DAEMON_LOGLEVEL, "SAI_LOG_LEVEL_NOTICE" }, { DAEMON_LOGOUTPUT, "SYSLOG" } });

    char* argvOne[] = {"loglevel", "-l", "INFO", "-c", "registered1"};
    optind = 0;
    EXPECT_EQ(swssloglevel(5, argvOne), EXIT_SUCCESS);
    checkLoglevel(db, "registered1", "INFO");
    EXPECT_FALSE(db.exists(std::string(CFG_LOGGER_TABLE_NAME) + "|registered2"));

    char* argvPrint[] = {"loglevel", "-p"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(2, argvPrint), EXIT_SUCCESS);
    std::string printed = testing::internal::GetCapturedStdout();
    // -p prints each component padded to 30 columns, then its level.
    auto line = [](const std::string& key, const std::string& level) {
        return key + std::string(30 - key.size(), ' ') + level + "\n";
    };
    EXPECT_NE(printed.find(line("registered1", "INFO")), std::string::npos) << printed;
    EXPECT_NE(printed.find(line("registered2", "NOTICE")), std::string::npos) << printed;
    EXPECT_NE(printed.find(line("SAI_API_registered3", "SAI_LOG_LEVEL_NOTICE")), std::string::npos) << printed;
    // registered1 has both a CONFIG_DB and a STATE_DB row and is listed once.
    const std::string listing = "\n" + printed;
    size_t first = listing.find("\nregistered1 ");
    ASSERT_NE(first, std::string::npos) << printed;
    EXPECT_EQ(listing.find("\nregistered1 ", first + 1), std::string::npos) << printed;

    char* argvDefault[] = {"loglevel", "-d"};
    optind = 0;
    EXPECT_EQ(swssloglevel(2, argvDefault), EXIT_SUCCESS);
    checkLoglevel(db, "registered1", "NOTICE");
    EXPECT_FALSE(db.exists(std::string(CFG_LOGGER_TABLE_NAME) + "|registered2"));
    EXPECT_FALSE(db.exists(std::string(CFG_LOGGER_TABLE_NAME) + "|SAI_API_registered3"));

    stateTable.del("registered1");
    stateTable.del("registered2");
    stateTable.del("SAI_API_registered3");
}

// -r deletes the whole configured row, which returns the component to its
// own default; -r -a removes every configured non-SAI row, -s -r -a the SAI ones.
TEST(LOGLEVEL, RemoveSetting)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    DBConnector stateDb("STATE_DB", 0);
    swss::Table stateTable(&stateDb, STATE_LOGGER_TABLE_NAME);
    stateTable.set("registered1", { { DAEMON_LOGLEVEL, "NOTICE" }, { DAEMON_LOGOUTPUT, "SYSLOG" } });

    swss::Table configTable(&db, CFG_LOGGER_TABLE_NAME);
    configTable.set("configured1", { { DAEMON_LOGLEVEL, "DEBUG" }, { DAEMON_LOGOUTPUT, "STDERR" } });
    configTable.set("configured2", { { DAEMON_LOGLEVEL, "INFO" } });
    configTable.set("SAI_API_configured3", { { DAEMON_LOGLEVEL, "SAI_LOG_LEVEL_DEBUG" } });

    auto exists = [&db](const std::string& key) {
        return db.exists(std::string(CFG_LOGGER_TABLE_NAME) + "|" + key);
    };

    // configured1 reloads its level only on SIGHUP, so -r says so.
    stateTable.set("configured1", { { DAEMON_LOGLEVEL, "DEBUG" }, { "require_manual_refresh", "true" } });
    char* argvOne[] = {"loglevel", "-r", "-c", "configured1"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(4, argvOne), EXIT_SUCCESS);
    EXPECT_NE(testing::internal::GetCapturedStdout().find("configured1 applies the change once it receives SIGHUP"),
              std::string::npos);
    stateTable.del("configured1");
    EXPECT_FALSE(exists("configured1"));
    EXPECT_TRUE(exists("configured2"));

    // A registered component without a row is already at its default, so
    // nothing changes and there is no SIGHUP to ask for.
    stateTable.hset("registered1", "require_manual_refresh", "true");
    char* argvRegistered[] = {"loglevel", "-r", "-c", "registered1"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(4, argvRegistered), EXIT_SUCCESS);
    EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
    EXPECT_FALSE(exists("registered1"));

    char* argvSai[] = {"loglevel", "-s", "-r", "-c", "configured3"};
    optind = 0;
    EXPECT_EQ(swssloglevel(5, argvSai), EXIT_SUCCESS);
    EXPECT_FALSE(exists("SAI_API_configured3"));
    EXPECT_TRUE(exists("configured2"));

    // -r -a and -s -r -a each give the SIGHUP hint for the rows they remove.
    configTable.set("SAI_API_configured3", { { DAEMON_LOGLEVEL, "SAI_LOG_LEVEL_DEBUG" } });
    stateTable.set("configured2", { { DAEMON_LOGLEVEL, "INFO" }, { "require_manual_refresh", "true" } });
    stateTable.set("SAI_API_configured3", { { DAEMON_LOGLEVEL, "SAI_LOG_LEVEL_DEBUG" }, { "require_manual_refresh", "true" } });
    const std::string hint2 = "configured2 applies the change once it receives SIGHUP\n";
    const std::string hint3 = "SAI_API_configured3 applies the change once it receives SIGHUP\n";
    char* argvAll[] = {"loglevel", "-r", "-a"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(3, argvAll), EXIT_SUCCESS);
    EXPECT_EQ(testing::internal::GetCapturedStdout(), hint2);
    EXPECT_FALSE(exists("configured2"));
    EXPECT_TRUE(exists("SAI_API_configured3"));

    char* argvAllSai[] = {"loglevel", "-s", "-r", "-a"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(4, argvAllSai), EXIT_SUCCESS);
    EXPECT_EQ(testing::internal::GetCapturedStdout(), hint3);
    EXPECT_FALSE(exists("SAI_API_configured3"));

    stateTable.del("configured2");
    stateTable.del("SAI_API_configured3");
    stateTable.del("registered1");
}

// -d and -r together are rejected and change nothing. swssloglevel exits, so
// it runs in a death-test child; the threadsafe style re-executes the test
// binary for that child.
TEST(LOGLEVEL, ResetAndRemoveRejected)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();
    swss::Table configTable(&db, CFG_LOGGER_TABLE_NAME);
    configTable.set("configured1", { { DAEMON_LOGLEVEL, "DEBUG" } });

    const std::string savedStyle = testing::FLAGS_gtest_death_test_style;
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    char* argvResetRemove[] = {"loglevel", "-d", "-r", "-a"};
    optind = 0;
    EXPECT_EXIT(swssloglevel(4, argvResetRemove), testing::ExitedWithCode(EXIT_FAILURE), "");
    testing::FLAGS_gtest_death_test_style = savedStyle;

    auto level = db.hget(std::string(CFG_LOGGER_TABLE_NAME) + "|configured1", DAEMON_LOGLEVEL);
    ASSERT_TRUE(level != nullptr);
    EXPECT_EQ(*level, "DEBUG");
}

// -r needs exactly one of -c and -a, and no -l, and -c must name a known
// component.
TEST(LOGLEVEL, RemoveRequiresOneTarget)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();
    swss::Table configTable(&db, CFG_LOGGER_TABLE_NAME);
    configTable.set("configured1", { { DAEMON_LOGLEVEL, "DEBUG" } });

    const std::string savedStyle = testing::FLAGS_gtest_death_test_style;
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    char* argvAlone[] = {"loglevel", "-r"};
    optind = 0;
    EXPECT_EXIT(swssloglevel(2, argvAlone), testing::ExitedWithCode(EXIT_FAILURE), "");
    char* argvBoth[] = {"loglevel", "-r", "-a", "-c", "configured1"};
    optind = 0;
    EXPECT_EXIT(swssloglevel(5, argvBoth), testing::ExitedWithCode(EXIT_FAILURE), "");
    char* argvLevel[] = {"loglevel", "-r", "-l", "DEBUG", "-c", "configured1"};
    optind = 0;
    EXPECT_EXIT(swssloglevel(6, argvLevel), testing::ExitedWithCode(EXIT_FAILURE), "");
    char* argvUnknown[] = {"loglevel", "-r", "-c", "no_such_component"};
    optind = 0;
    EXPECT_EXIT(swssloglevel(4, argvUnknown), testing::ExitedWithCode(EXIT_FAILURE), "");
    testing::FLAGS_gtest_death_test_style = savedStyle;

    EXPECT_TRUE(db.exists(std::string(CFG_LOGGER_TABLE_NAME) + "|configured1"));
}

// -l -c and -d say when a target applies the change only on SIGHUP, as -r does.
TEST(LOGLEVEL, SetReportsManualRefresh)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    DBConnector stateDb("STATE_DB", 0);
    swss::Table stateTable(&stateDb, STATE_LOGGER_TABLE_NAME);
    stateTable.set("refreshed1", { { DAEMON_LOGLEVEL, "NOTICE" }, { "require_manual_refresh", "true" } });
    stateTable.set("plain1", { { DAEMON_LOGLEVEL, "NOTICE" } });
    const std::string hint = "refreshed1 applies the change once it receives SIGHUP";

    char* argvSet[] = {"loglevel", "-l", "DEBUG", "-c", "refreshed1"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(5, argvSet), EXIT_SUCCESS);
    EXPECT_NE(testing::internal::GetCapturedStdout().find(hint), std::string::npos);

    char* argvPlain[] = {"loglevel", "-l", "DEBUG", "-c", "plain1"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(5, argvPlain), EXIT_SUCCESS);
    EXPECT_EQ(testing::internal::GetCapturedStdout(), "");

    char* argvDefault[] = {"loglevel", "-d"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(2, argvDefault), EXIT_SUCCESS);
    std::string printed = testing::internal::GetCapturedStdout();
    EXPECT_NE(printed.find(hint), std::string::npos) << printed;
    EXPECT_EQ(printed.find("plain1"), std::string::npos) << printed;

    stateTable.del("refreshed1");
    stateTable.del("plain1");
}

// -p lists a component that has only a CONFIG_DB LOGGER row, whatever wrote
// it, at its configured level.
TEST(LOGLEVEL, PrintConfiguredOnly)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();
    setLoglevel(db, "legacy1", "WARN");

    char* argvPrint[] = {"loglevel", "-p"};
    optind = 0;
    testing::internal::CaptureStdout();
    EXPECT_EQ(swssloglevel(2, argvPrint), EXIT_SUCCESS);
    std::string printed = testing::internal::GetCapturedStdout();
    EXPECT_NE(printed.find("legacy1" + std::string(30 - 7, ' ') + "WARN\n"), std::string::npos) << printed;
}

// A STATE_DB read that fails after the key listing is reported once, and -p
// goes on without STATE_DB: configured components are still printed, and
// components only STATE_DB listed are skipped rather than reported as errors.
TEST(LOGLEVEL, StateDbFailsMidCommand)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();
    setLoglevel(db, "configured1", "INFO");

    // A string where a hash is expected is listed by the key scan but fails
    // HGET with WRONGTYPE. It sorts first, so the later reads find no table.
    DBConnector stateDb("STATE_DB", 0);
    const std::string broken = std::string(STATE_LOGGER_TABLE_NAME) + "|broken1";
    stateDb.set(broken, "not-a-hash");
    swss::Table stateTable(&stateDb, STATE_LOGGER_TABLE_NAME);
    stateTable.set("registered1", { { DAEMON_LOGLEVEL, "NOTICE" } });

    char* argvPrint[] = {"loglevel", "-p"};
    optind = 0;
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = swssloglevel(2, argvPrint);
    std::string out = testing::internal::GetCapturedStdout();
    std::string err = testing::internal::GetCapturedStderr();

    EXPECT_EQ(rc, EXIT_SUCCESS) << err;
    EXPECT_NE(out.find("configured1" + std::string(30 - 11, ' ') + "INFO\n"), std::string::npos) << out;
    EXPECT_EQ(out.find("registered1"), std::string::npos) << out;
    EXPECT_NE(err.find("STATE_DB unavailable, continuing without it"), std::string::npos) << err;
    EXPECT_EQ(err.find("Unknown log level"), std::string::npos) << err;

    stateDb.del(broken);
    stateTable.del("registered1");
}

// Child side of StateDbUnavailable: exits 0 when -p lists and -l -c sets the
// configured component.
[[ noreturn ]] static void runWithoutStateDb(const char* path)
{
    SonicDBConfig::reset();
    SonicDBConfig::initialize(path);

    char* argvPrint[] = {"loglevel", "-p"};
    optind = 0;
    testing::internal::CaptureStdout();
    int printed = swssloglevel(2, argvPrint);
    bool listed = testing::internal::GetCapturedStdout().find("configured1") != std::string::npos;

    if (printed != EXIT_SUCCESS || !listed)
    {
        _exit(1);
    }

    // -c returns, where -a exits, so the result can be checked here.
    char* argvSet[] = {"loglevel", "-l", "DEBUG", "-c", "configured1"};
    optind = 0;
    int set = swssloglevel(5, argvSet);
    DBConnector configDb("CONFIG_DB", 0);
    auto level = configDb.hget(std::string(CFG_LOGGER_TABLE_NAME) + "|configured1", DAEMON_LOGLEVEL);

    _exit(set == EXIT_SUCCESS && level && *level == "DEBUG" ? 0 : 2);
}

// Without STATE_DB, swssloglevel still lists and sets the configured
// components. Runs in a child, which points STATE_DB at a dead socket.
TEST(LOGLEVEL, StateDbUnavailable)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();
    swss::Table configTable(&db, CFG_LOGGER_TABLE_NAME);
    configTable.set("configured1", { { DAEMON_LOGLEVEL, "INFO" } });

    // The child is re-executed and runs this test again up to EXPECT_EXIT,
    // so the file has a fixed name.
    const char* path = "/tmp/swss_loglevel_ut_state_db_down.json";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    ASSERT_GE(fd, 0);
    const std::string config = R"({
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

    // Re-executed, as in ResetAndRemoveRejected.
    const std::string savedStyle = testing::FLAGS_gtest_death_test_style;
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    EXPECT_EXIT(runWithoutStateDb(path), testing::ExitedWithCode(0), "STATE_DB unavailable");
    testing::FLAGS_gtest_death_test_style = savedStyle;

    unlink(path);
}

// Child side of SetAllIncludesRegistered. -a exits, so stdout goes to stderr
// where EXPECT_EXIT can match it.
[[ noreturn ]] static void setAllToDebug()
{
    dup2(STDERR_FILENO, STDOUT_FILENO);
    char* argvAll[] = {"loglevel", "-l", "DEBUG", "-a"};
    optind = 0;
    swssloglevel(4, argvAll);
    _exit(3);
}

// -l -a sets every non-SAI component, including one registered only in
// STATE_DB, and says when a target applies the change only on SIGHUP.
TEST(LOGLEVEL, SetAllIncludesRegistered)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();
    setLoglevel(db, "configured1", "INFO");
    setLoglevel(db, "SAI_API_configured2", "SAI_LOG_LEVEL_INFO");

    DBConnector stateDb("STATE_DB", 0);
    swss::Table stateTable(&stateDb, STATE_LOGGER_TABLE_NAME);
    stateTable.set("registered1", { { DAEMON_LOGLEVEL, "NOTICE" }, { "require_manual_refresh", "true" } });

    // Re-executed, as in ResetAndRemoveRejected.
    const std::string savedStyle = testing::FLAGS_gtest_death_test_style;
    testing::FLAGS_gtest_death_test_style = "threadsafe";
    EXPECT_EXIT(setAllToDebug(), testing::ExitedWithCode(EXIT_SUCCESS),
                "registered1 applies the change once it receives SIGHUP");
    testing::FLAGS_gtest_death_test_style = savedStyle;

    checkLoglevel(db, "configured1", "DEBUG");
    checkLoglevel(db, "registered1", "DEBUG");
    checkLoglevel(db, "SAI_API_configured2", "SAI_LOG_LEVEL_INFO");

    stateTable.del("registered1");
}
