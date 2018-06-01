#include "common/dbconnector.h"
#include "common/consumertable.h"
#include "common/notificationconsumer.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/selectabletimer.h"
#include "common/subscriberstatetable.h"
#include "common/netmsg.h"
#include "common/netlink.h"
#include "gtest/gtest.h"


using namespace std;
using namespace swss;


#define TEST_VIEW            (7)
#define DEFAULT_POP_BATCH_SIZE (10)


TEST(Priority, default_pri_values)
{
    std::string tableName = "tableName";

    DBConnector db(TEST_VIEW, "localhost", 6379, 0);

    timespec interval = { .tv_sec = 1, .tv_nsec = 0 };

    NetLink nl;
    ConsumerStateTable cst(&db, tableName);
    ConsumerTable ct(&db, tableName);
    NotificationConsumer nc(&db, tableName);
    RedisSelect rs;
    SelectableEvent se;
    SelectableTimer st(interval);
    SubscriberStateTable sst(&db, tableName);

    EXPECT_EQ(nl.getPri(), 0);
    EXPECT_EQ(cst.getPri(), 0);
    EXPECT_EQ(ct.getPri(), 0);
    EXPECT_EQ(nc.getPri(), 100);
    EXPECT_EQ(rs.getPri(), 0);
    EXPECT_EQ(se.getPri(), 0);
    EXPECT_EQ(st.getPri(), 50);
    EXPECT_EQ(sst.getPri(), 0);
}

TEST(Priority, set_pri_values)
{
    std::string tableName = "tableName";

    DBConnector db(TEST_VIEW, "localhost", 6379, 0);

    timespec interval = { .tv_sec = 1, .tv_nsec = 0 };

    NetLink nl(101);
    ConsumerStateTable cst(&db, tableName, DEFAULT_POP_BATCH_SIZE, 102);
    ConsumerTable ct(&db, tableName, DEFAULT_POP_BATCH_SIZE, 103);
    NotificationConsumer nc(&db, tableName, 104);
    RedisSelect rs(105);
    SelectableEvent se(106);
    SelectableTimer st(interval, 107);
    SubscriberStateTable sst(&db, tableName, DEFAULT_POP_BATCH_SIZE, 108);

    EXPECT_EQ(nl.getPri(), 101);
    EXPECT_EQ(cst.getPri(), 102);
    EXPECT_EQ(ct.getPri(), 103);
    EXPECT_EQ(nc.getPri(), 104);
    EXPECT_EQ(rs.getPri(), 105);
    EXPECT_EQ(se.getPri(), 106);
    EXPECT_EQ(st.getPri(), 107);
    EXPECT_EQ(sst.getPri(), 108);
}

TEST(Priority, priority_select_1)
{
    Select cs;
    Selectable *selectcs;

    SelectableEvent s1(100);
    SelectableEvent s2(1000);
    SelectableEvent s3(10000);

    cs.addSelectable(&s1);
    cs.addSelectable(&s2);
    cs.addSelectable(&s3);

    s1.notify();
    s2.notify();
    s3.notify();

    int ret;
    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s3);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s2);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s1);
}

TEST(Priority, priority_select_2)
{
    Select cs;
    Selectable *selectcs;

    SelectableEvent s1(100);
    SelectableEvent s2(1000);
    SelectableEvent s3(10000);

    s1.notify();
    s2.notify();
    s3.notify();

    cs.addSelectable(&s1);
    cs.addSelectable(&s2);
    cs.addSelectable(&s3);

    int ret;
    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s3);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s2);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s1);
}

TEST(Priority, priority_select_3)
{
    Select cs;
    Selectable *selectcs;

    SelectableEvent s1(10);
    SelectableEvent s2(10);
    SelectableEvent s3(10000);

    cs.addSelectable(&s1);
    cs.addSelectable(&s2);
    cs.addSelectable(&s3);

    s1.notify();
    s2.notify();
    s3.notify();

    int ret;
    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s3);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_TRUE(selectcs==&s1 || selectcs==&s2);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_TRUE(selectcs==&s1 || selectcs==&s2);
}

TEST(Priority, priority_select_4)
{
    Select cs;
    Selectable *selectcs;

    SelectableEvent s1(10);
    SelectableEvent s2(10000);
    SelectableEvent s3(10000);

    cs.addSelectable(&s1);
    cs.addSelectable(&s2);
    cs.addSelectable(&s3);

    s1.notify();
    s2.notify();
    s3.notify();

    int ret;
    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_TRUE(selectcs==&s2 || selectcs==&s3);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_TRUE(selectcs==&s2 || selectcs==&s3);

    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s1);
}

TEST(Priority, priority_select_5)
{
    Select cs;
    Selectable *selectcs;

    SelectableEvent s1(150);
    SelectableEvent s2(1000);

    cs.addSelectable(&s1);
    cs.addSelectable(&s2);

    s1.notify();
    s2.notify();

    int ret;
    ret = cs.select(&selectcs);
    EXPECT_EQ(ret, Select::OBJECT);
    EXPECT_EQ(selectcs, &s2);

    cs.removeSelectable(&s1);

    ret = cs.select(&selectcs, 1000);
    EXPECT_EQ(ret, Select::TIMEOUT);
}

TEST(Priority, priority_select_6)
{
    Select cs;
    Selectable *selectcs1;
    Selectable *selectcs2;

    SelectableEvent s1(1000);
    SelectableEvent s2(1000);

    cs.addSelectable(&s1);
    cs.addSelectable(&s2);

    s1.notify();
    s2.notify();

    int ret;
    ret = cs.select(&selectcs1);
    EXPECT_EQ(ret, Select::OBJECT);

    s1.notify();
    s2.notify();

    ret = cs.select(&selectcs2);
    EXPECT_EQ(ret, Select::OBJECT);
    // we gave fair scheduler. we've read different selectables on the second read
    EXPECT_NE(selectcs1, selectcs2);
}
