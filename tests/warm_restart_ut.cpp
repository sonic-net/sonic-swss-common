#include <iostream>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/table.h"
#include "common/schema.h"
#include "common/warm_restart.h"

using namespace std;
using namespace swss;

static const string testAppName = "TestApp";
static const string testDockerName = "TestDocker";

TEST(WarmRestart, checkWarmStart_and_State)
{
    DBConnector stateDb(STATE_DB, "localhost", 6379, 0);
    Table stateWarmRestartTable(&stateDb, STATE_WARM_RESTART_TABLE_NAME);

    DBConnector configDb(CONFIG_DB, "localhost", 6379, 0);
    Table cfgWarmRestartTable(&configDb, CFG_WARM_RESTART_TABLE_NAME);

    //Clean up warm restart state for testAppName and warm restart config for testDockerName
    stateWarmRestartTable.del(testAppName);
    cfgWarmRestartTable.del(testDockerName);


    //Initialize WarmStart class for TestApp
    WarmStart::initialize(testAppName, testDockerName, 0, "localhost", 6379);

    // Application is supposed to set the warm start state explicitly based on
    // its progress of state restore.
    WarmStart::setWarmStartState(testAppName, WarmStart::INITIALIZED);

    // state check is for external observation.
    string state;
    stateWarmRestartTable.hget(testAppName, "state", state);
    EXPECT_EQ(state, "initialized");

    // perform checkWarmStart for TestAPP running in TestDocker
    WarmStart::checkWarmStart(testAppName, testDockerName);


    // The "restore_count" for TestApp in stateDB should be 0 since
    // warm restart has not been enabled for "system" or "TestDocker"
    string value;
    stateWarmRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "0");

    // warm restart is disabled by default.
    bool enabled = WarmStart::isWarmStart();
    EXPECT_FALSE(enabled);

    // enable system level warm restart.
    cfgWarmRestartTable.hset("system", "enable", "true");

    // Do checkWarmStart for TestAPP running in TestDocker again.

    // Note, this is for unit testing only. checkWarmStart() is supposed to be
    // called only at the very start of a process, or when the re-initialization of
    // a process is to be performed.
    WarmStart::checkWarmStart(testAppName, testDockerName);

    // After restoring data from DB, application reaches RESTORED state.
    WarmStart::setWarmStartState(testAppName, WarmStart::RESTORED);

    // Restore count should have incremented by 1.
    stateWarmRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "1");

    // state check is for external observation.
    stateWarmRestartTable.hget(testAppName, "state", state);
    EXPECT_EQ(state, "restored");

    enabled = WarmStart::isWarmStart();
    EXPECT_TRUE(enabled);

    // Usually application will sync up with latest external env as to data state,
    // after that it should set the state to RECONCILED for the observation of external tools.
    WarmStart::setWarmStartState(testAppName, WarmStart::RECONCILED);

    // state check is for external observation.
    stateWarmRestartTable.hget(testAppName, "state", state);
    EXPECT_EQ(state, "reconciled");


    // If the restore_count of STATE_WARM_RESTART_TABLE_NAME table has been flushed from stateDB
    // for the specific application, we assume
    // that all the warm restore data are not available either, the application should not
    // perform warm restart, even though it is configured to do so.
    stateWarmRestartTable.del(testAppName);

    // This is to simulate application restart, upon restart checkWarmStart() is to called at the beginning.
    WarmStart::checkWarmStart(testAppName, testDockerName);

    // restore_count for the application is set to 0 again.
    stateWarmRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "0");

    enabled = WarmStart::isWarmStart();
    EXPECT_FALSE(enabled);


    // disable system level warm restart.
    cfgWarmRestartTable.hset("system", "enable", "false");
    // Enable docker level warm restart.
    cfgWarmRestartTable.hset(testDockerName, "enable", "true");

    // Note, this is for unit testing only. checkWarmStart() is supposed to be
    // called only at the very start of a process, or when the re-initialization of
    // a process is to be performed.
    WarmStart::checkWarmStart(testAppName, testDockerName);

    // Restore count should have incremented by 1.
    stateWarmRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "1");

    enabled = WarmStart::isWarmStart();
    EXPECT_TRUE(enabled);
}



TEST(WarmRestart, getWarmStartTimer)
{
    DBConnector configDb(CONFIG_DB, "localhost", 6379, 0);
    Table cfgWarmRestartTable(&configDb, CFG_WARM_RESTART_TABLE_NAME);

    //Initialize WarmStart class for TestApp
    WarmStart::initialize(testAppName, testDockerName, 0, "localhost", 6379);

    uint32_t timer;
    // By default, no default warm start timer exists in configDB for any application
    // value 0 is returned for empty configuration.
    timer = WarmStart::getWarmStartTimer(testAppName, testDockerName);
    EXPECT_EQ(timer, 0u);

    // config manager is supposed to make the setting.
    cfgWarmRestartTable.hset(testDockerName, testAppName + "_timer", "5000");

    timer = WarmStart::getWarmStartTimer(testAppName, testDockerName);

    EXPECT_EQ(timer, 5000u);
}