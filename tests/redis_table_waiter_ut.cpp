#include <gtest/gtest.h>
#include <thread>
#include <string>
#include <unistd.h>

#include "common/dbconnector.h"
#include "common/redis_table_waiter.h"
#include "common/table.h"

using namespace swss;
using namespace std;

const std::string dbName = "STATE_DB";
const std::string tableName = "TestTable";
const std::string key = "testKey";
const std::string field = "testField";
const std::string setValue = "1234";

static void set_field(int delay)
{
    if (delay > 0)
    {
        sleep(delay);
    }

    DBConnector db(dbName, 0);
    Table table(&db, tableName);
    table.hset(key, field, setValue);
}

static void del_key(int delay)
{
    if (delay > 0)
    {
        sleep(delay);
    }

    DBConnector db(dbName, 0);
    Table table(&db, tableName);
    table.del(key);
}

TEST(RedisTableWaiter, waitUntilFieldSet)
{
    del_key(0);
    DBConnector db(dbName, 0);
    RedisTableWaiter::ConditionFunc condFunc = [&](const std::string &value) -> bool {
        return value == setValue;
    };
    thread t(set_field, 1);
    auto ret = RedisTableWaiter::waitUntilFieldSet(db,
        tableName,
        key,
        field,
        3,
        condFunc);
    EXPECT_TRUE(ret);
    t.join();
    // field already set
    ret = RedisTableWaiter::waitUntilFieldSet(db,
        tableName,
        key,
        field,
        3,
        condFunc);
    EXPECT_TRUE(ret);
}

TEST(RedisTableWaiter, waitUntilKeySet)
{
    del_key(0);
    DBConnector db(dbName, 0);
    thread t(set_field, 1);
    auto ret = RedisTableWaiter::waitUntilKeySet(db,
        tableName,
        key,
        3);
    EXPECT_TRUE(ret);
    t.join();
    // key already exist
    ret = RedisTableWaiter::waitUntilKeySet(db,
        tableName,
        key,
        3);
    EXPECT_TRUE(ret);
}

TEST(RedisTableWaiter, waitUntilKeyDel)
{
    set_field(0);
    DBConnector db(dbName, 0);
    thread t(del_key, 1);
    auto ret = RedisTableWaiter::waitUntilKeyDel(db,
        tableName,
        key,
        3);
    EXPECT_TRUE(ret);
    t.join();
    // key already removed
    ret = RedisTableWaiter::waitUntilKeyDel(db,
        tableName,
        key,
        3);
    EXPECT_TRUE(ret);
}
