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

static const string FAST_REBOOT_KEY = "FAST_RESTART_ENABLE_TABLE|system";

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
        db.hset(FAST_REBOOT_KEY, "enable", "true");
    }

    ~FastBootHelper()
    {
        db.hset(FAST_REBOOT_KEY, "enable", "false");
    }
private:
    DBConnector db;
};

TEST(RestartWaiter, success)
{
    set_reboot_status("true");
    thread t(set_reboot_status, "false", 3);
    EXPECT_TRUE(RestartWaiter::waitAdvancedBootDone());
    t.join();
}

TEST(RestartWaiter, successWarmReboot)
{
    set_reboot_status("true");
    thread t(set_reboot_status, "false", 3);
    EXPECT_TRUE(RestartWaiter::waitWarmBootDone());
    t.join();
}

TEST(RestartWaiter, successFastReboot)
{
    FastBootHelper helper;
    set_reboot_status("true");
    thread t(set_reboot_status, "false", 3);
    EXPECT_TRUE(RestartWaiter::waitFastBootDone());
    t.join();
}

TEST(RestartWaiter, timeout)
{
    set_reboot_status("true");
    EXPECT_FALSE(RestartWaiter::waitAdvancedBootDone(1));
    EXPECT_FALSE(RestartWaiter::waitWarmBootDone(1));

    FastBootHelper helper;
    EXPECT_FALSE(RestartWaiter::waitFastBootDone(1));

    set_reboot_status("false");
}

TEST(RestartWaiter, successNoDelay)
{
    set_reboot_status("false");
    EXPECT_TRUE(RestartWaiter::waitAdvancedBootDone());
    EXPECT_TRUE(RestartWaiter::waitWarmBootDone());

    FastBootHelper helper;
    EXPECT_TRUE(RestartWaiter::waitFastBootDone());
}

TEST(RestartWaiter, successNoKey)
{
    DBConnector db("STATE_DB", 0);
    string key = string(STATE_WARM_RESTART_ENABLE_TABLE_NAME) + string("|system");
    db.del({key});
    EXPECT_TRUE(RestartWaiter::waitAdvancedBootDone());
    EXPECT_TRUE(RestartWaiter::waitWarmBootDone());

    FastBootHelper helper;
    EXPECT_TRUE(RestartWaiter::waitFastBootDone());
}

TEST(RestartWaiter, waitWarmButFastInProgress)
{
    FastBootHelper helper;
    EXPECT_TRUE(RestartWaiter::waitWarmBootDone());
}

TEST(RestartWaiter, waitFastButFastNotInProgress)
{
    EXPECT_TRUE(RestartWaiter::waitFastBootDone());
}
