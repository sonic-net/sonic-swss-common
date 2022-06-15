#include <iostream>

#include "../common/advanced_restart.h"
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/table.h"
#include "common/schema.h"

using namespace std;
using namespace swss;

static const string testAppName = "TestApp";
static const string testDockerName = "TestDocker";

TEST(AdvancedRestart, checkAdvancedStart_and_State)
{
    DBConnector stateDb("STATE_DB", 0, true);
    Table stateAdvancedRestartTable(&stateDb, STATE_ADVANCED_RESTART_TABLE_NAME);
    Table stateAdvancedRestartEnableTable(&stateDb, STATE_ADVANCED_RESTART_ENABLE_TABLE_NAME);

    DBConnector configDb("CONFIG_DB", 0, true);
    Table cfgAdvancedRestartTable(&configDb, CFG_ADVANCED_RESTART_TABLE_NAME);

    //Clean up advanced restart state for testAppName and advanced restart config for testDockerName
    stateAdvancedRestartTable.del(testAppName);
    cfgAdvancedRestartTable.del(testDockerName);


    //Initialize AdvancedStart class for TestApp
    AdvancedStart::initialize(testAppName, testDockerName, 0, true);

    // Application is supposed to set the advanced start state explicitly based on
    // its progress of state restore.
    AdvancedStart::setAdvancedStartState(testAppName, AdvancedStart::INITIALIZED);

    // state check is for external observation.
    string state;
    stateAdvancedRestartTable.hget(testAppName, "state", state);
    EXPECT_EQ(state, "initialized");

    // perform checkAdvancedStart for TestAPP running in TestDocker
    AdvancedStart::checkAdvancedStart(testAppName, testDockerName);


    // The "restore_count" for TestApp in stateDB should be 0 since
    // advanced restart has not been enabled for "system" or "TestDocker"
    string value;
    stateAdvancedRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "0");

    // advanced restart is disabled by default.
    bool enabled = AdvancedStart::isAdvancedStart();
    EXPECT_FALSE(enabled);

    // system advanced reboot is disabled
    bool system_enabled = AdvancedStart::isSystemAdvancedRebootEnabled();
    EXPECT_FALSE(system_enabled);

    // enable system level advanced restart.
    stateAdvancedRestartEnableTable.hset("system", "enable", "true");

    // Do checkAdvancedStart for TestAPP running in TestDocker again.

    // Note, this is for unit testing only. checkAdvancedStart() is supposed to be
    // called only at the very start of a process, or when the re-initialization of
    // a process is to be performed.
    AdvancedStart::checkAdvancedStart(testAppName, testDockerName);

    // After restoring data from DB, application reaches RESTORED state.
    AdvancedStart::setAdvancedStartState(testAppName, AdvancedStart::RESTORED);

    // Restore count should have incremented by 1.
    stateAdvancedRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "1");

    // state check is for external observation.
    stateAdvancedRestartTable.hget(testAppName, "state", state);
    EXPECT_EQ(state, "restored");

    enabled = AdvancedStart::isAdvancedStart();
    EXPECT_TRUE(enabled);

    system_enabled = AdvancedStart::isSystemAdvancedRebootEnabled();
    EXPECT_TRUE(system_enabled);

    // Usually application will sync up with latest external env as to data state,
    // after that it should set the state to RECONCILED for the observation of external tools.
    AdvancedStart::setAdvancedStartState(testAppName, AdvancedStart::RECONCILED);

    // state check is for external observation.
    stateAdvancedRestartTable.hget(testAppName, "state", state);
    EXPECT_EQ(state, "reconciled");


    // If the restore_count of STATE_ADVANCED_RESTART_TABLE_NAME table has been flushed from stateDB
    // for the specific application, we assume
    // that all the advanced restore data are not available either, the application should not
    // perform advanced restart, even though it is configured to do so.
    stateAdvancedRestartTable.del(testAppName);

    // This is to simulate application restart, upon restart checkAdvancedStart() is to called at the beginning.
    AdvancedStart::checkAdvancedStart(testAppName, testDockerName);

    // restore_count for the application is set to 0 again.
    stateAdvancedRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "0");

    enabled = AdvancedStart::isAdvancedStart();
    EXPECT_FALSE(enabled);


    // disable system level advanced restart.
    stateAdvancedRestartEnableTable.hset("system", "enable", "false");
    // Enable docker level advanced restart.
    stateAdvancedRestartEnableTable.hset(testDockerName, "enable", "true");

    // Note, this is for unit testing only. checkAdvancedStart() is supposed to be
    // called only at the very start of a process, or when the re-initialization of
    // a process is to be performed.
    AdvancedStart::checkAdvancedStart(testAppName, testDockerName);

    // Restore count should have incremented by 1.
    stateAdvancedRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "1");

    // Test checkAdvancedStart function without increment restore count
    AdvancedStart::checkAdvancedStart(testAppName, testDockerName, false);

    // Restore count should still be 1.
    stateAdvancedRestartTable.hget(testAppName, "restore_count", value);
    EXPECT_EQ(value, "1");

    enabled = AdvancedStart::isAdvancedStart();
    EXPECT_TRUE(enabled);

    system_enabled = AdvancedStart::isSystemAdvancedRebootEnabled();
    EXPECT_FALSE(system_enabled);
}



TEST(AdvancedRestart, getAdvancedStartTimer)
{
    DBConnector configDb("CONFIG_DB", 0, true);
    Table cfgAdvancedRestartTable(&configDb, CFG_ADVANCED_RESTART_TABLE_NAME);

    //Initialize AdvancedStart class for TestApp
    AdvancedStart::initialize(testAppName, testDockerName, 0, true);

    uint32_t timer;
    // By default, no default advanced start timer exists in configDB for any application
    // value 0 is returned for empty configuration.
    timer = AdvancedStart::getAdvancedStartTimer(testAppName, testDockerName);
    EXPECT_EQ(timer, 0u);

    // config manager is supposed to make the setting.
    cfgAdvancedRestartTable.hset(testDockerName, testAppName + "_timer", "5000");

    timer = AdvancedStart::getAdvancedStartTimer(testAppName, testDockerName);

    EXPECT_EQ(timer, 5000u);
}

TEST(AdvancedRestart, set_get_DataCheckState)
{
    DBConnector stateDb("STATE_DB", 0, true);
    Table stateAdvancedRestartTable(&stateDb, STATE_ADVANCED_RESTART_TABLE_NAME);

    //Clean up advanced restart state for testAppName
    stateAdvancedRestartTable.del(testAppName);

    //Initialize AdvancedStart class for TestApp
    AdvancedStart::initialize(testAppName, testDockerName, 0, true);

    AdvancedStart::DataCheckState state;
    // basic state set check for shutdown stage
    string value;
    bool ret = stateAdvancedRestartTable.hget(testAppName, "shutdown_check", value);
    EXPECT_FALSE(ret);
    EXPECT_EQ(value, "");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN);
    EXPECT_EQ(state, AdvancedStart::CHECK_IGNORED);


    AdvancedStart::setDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN, AdvancedStart::CHECK_IGNORED);
    ret = stateAdvancedRestartTable.hget(testAppName, "shutdown_check", value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "ignored");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN);
    EXPECT_EQ(state, AdvancedStart::CHECK_IGNORED);


    AdvancedStart::setDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN, AdvancedStart::CHECK_PASSED);
    ret = stateAdvancedRestartTable.hget(testAppName, "shutdown_check", value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "passed");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN);
    EXPECT_EQ(state, AdvancedStart::CHECK_PASSED);


    AdvancedStart::setDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN, AdvancedStart::CHECK_FAILED);

    ret = stateAdvancedRestartTable.hget(testAppName, "shutdown_check", value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "failed");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_SHUTDOWN);
    EXPECT_EQ(state, AdvancedStart::CHECK_FAILED);


    // basic state set check for restore stage
    ret = stateAdvancedRestartTable.hget(testAppName, "restore_check", value);
    EXPECT_FALSE(ret);
    EXPECT_EQ(value, "");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE);
    EXPECT_EQ(state, AdvancedStart::CHECK_IGNORED);

    AdvancedStart::setDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE, AdvancedStart::CHECK_IGNORED);
    ret = stateAdvancedRestartTable.hget(testAppName, "restore_check", value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "ignored");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE);
    EXPECT_EQ(state, AdvancedStart::CHECK_IGNORED);

    AdvancedStart::setDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE, AdvancedStart::CHECK_PASSED);
    ret = stateAdvancedRestartTable.hget(testAppName, "restore_check", value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "passed");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE);
    EXPECT_EQ(state, AdvancedStart::CHECK_PASSED);


    AdvancedStart::setDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE, AdvancedStart::CHECK_FAILED);
    ret = stateAdvancedRestartTable.hget(testAppName, "restore_check", value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "failed");
    state = AdvancedStart::getDataCheckState(testAppName, AdvancedStart::STAGE_RESTORE);
    EXPECT_EQ(state, AdvancedStart::CHECK_FAILED);
}
