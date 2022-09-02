#include <gtest/gtest.h>
#include <thread>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/dbconnector.h"
#include "common/restart_waiter.h"
#include "common/schema.h"
#include "common/table.h"

using namespace swss;
using namespace std;

static const string FAST_REBOOT_KEY = "FAST_REBOOT|system";

static void set_reboot_status(string status, int delay = 0)
{
    if (delay > 0)
    {
        sleep(delay);
    }

    DBConnector db("STATE_DB", 0);
    Table table(&db, STATE_WARM_RESTART_ENABLE_TABLE_NAME);
    table.hset("system", "enable", status);
}

class FastBootHelper
{
public:
    FastBootHelper(): db("STATE_DB", 0)
    {
        db.set(FAST_REBOOT_KEY, "1");
    }

    ~FastBootHelper()
    {
        db.del({FAST_REBOOT_KEY});
    }
private:
    DBConnector db;
};

TEST(RestartWaiter, success)
{
    set_reboot_status("true");
    thread t(set_reboot_status, "false", 3);
    EXPECT_TRUE(RestartWaiter::waitRestartDone());
    t.join();
}

TEST(RestartWaiter, successWarmRestart)
{
    set_reboot_status("true");
    thread t(set_reboot_status, "false", 3);
    EXPECT_TRUE(RestartWaiter::waitWarmRestartDone());
    t.join();
}

TEST(RestartWaiter, successFastRestart)
{
    FastBootHelper helper;
    set_reboot_status("true");
    thread t(set_reboot_status, "false", 3);
    EXPECT_TRUE(RestartWaiter::waitFastRestartDone());
    t.join();
}

TEST(RestartWaiter, timeout)
{
    set_reboot_status("true");
    EXPECT_FALSE(RestartWaiter::waitRestartDone(1));
    EXPECT_FALSE(RestartWaiter::waitWarmRestartDone(1));

    FastBootHelper helper;
    EXPECT_FALSE(RestartWaiter::waitFastRestartDone(1));

    set_reboot_status("false");
}

TEST(RestartWaiter, successNoDelay)
{
    set_reboot_status("false");
    EXPECT_TRUE(RestartWaiter::waitRestartDone());
    EXPECT_TRUE(RestartWaiter::waitWarmRestartDone());

    FastBootHelper helper;
    EXPECT_TRUE(RestartWaiter::waitFastRestartDone());
}

TEST(RestartWaiter, successNoKey)
{
    DBConnector db("STATE_DB", 0);
    string key = string(STATE_WARM_RESTART_ENABLE_TABLE_NAME) + string("|system");
    db.del({key});
    EXPECT_TRUE(RestartWaiter::waitRestartDone());
    EXPECT_TRUE(RestartWaiter::waitWarmRestartDone());

    FastBootHelper helper;
    EXPECT_TRUE(RestartWaiter::waitFastRestartDone());
}

TEST(RestartWaiter, waitWarmButFastInProgress)
{
    FastBootHelper helper;
    EXPECT_TRUE(RestartWaiter::waitWarmRestartDone());
}

TEST(RestartWaiter, waitFastButFastNotInProgress)
{
    EXPECT_TRUE(RestartWaiter::waitFastRestartDone());
}
