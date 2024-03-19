#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/table.h"
#include "common/subscriberstatetable.h"
#include "common/tuplehelper.h"

using namespace std;
using namespace swss;

#define NUMBER_OF_THREADS   (64) // Spawning more than 256 threads causes libc++ to except
#define NUMBER_OF_OPS     (1000)
#define MAX_FIELDS_DIV      (30) // Testing up to 30 fields objects
#define PRINT_SKIP          (10) // Print + for Producer and - for Subscriber for every 100 ops

static const string testTableName = "UT_REDIS_TABLE";
static const string testTableName2 = "UT_REDIS_TABLE2";

static inline string key(int index, int keyid)
{
    return string("key_") + to_string(index) + ":" + to_string(keyid);
}

static inline string field(int index, int keyid)
{
    return string("field ") + to_string(index) + ":" + to_string(keyid);
}

static inline string value(int index, int keyid)
{
    if (keyid == 0)
    {
        return string(); // empty
    }

    return string("value ") + to_string(index) + ":" + to_string(keyid);
}


static inline void clearDB()
{
    DBConnector db("TEST_DB", 0, true);
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);
    r.checkStatusOK();
}

TEST(SelectHelper, select)
{
    clearDB();

    /* Prepare producer */
    DBConnector db("TEST_DB", 0, true);
    Table p(&db, "MID_PLANE_BRIDGE");

    /* Prepare subscriber */
    SubscriberStateTable c(&db, "MID_PLANE_BRIDGE");
    Select cs;
    cs.addSelectable(&c);
    SelectHelper helper;

    /* Set operation */
    p.hset("GLOBAL", "bridge", "bridge_midplane");

    /* Pop operation */
    helper.DoSelect(cs);
    EXPECT_EQ(helper.GetResult(), Select::OBJECT);
    KeyOpFieldsValuesTuple kco;
    c.pop(kco);
    EXPECT_EQ(kfvKey(kco), "GLOBAL");
    EXPECT_EQ(kfvOp(kco), "SET");

    auto fvs = kfvFieldsValues(kco);
    EXPECT_EQ(fvs.size(), (unsigned int)(1));

    EXPECT_EQ(fvField(fvs[0]), "bridge");
    EXPECT_EQ(fvValue(fvs[0]), "bridge_midplane");
}

TEST(TupleHelper, select)
{
    KeyOpFieldsValuesTuple tuple{
        "Key",
        "SET",
        std::vector<FieldValueTuple>{
            {"Field", "Value"}
        }};
    
    EXPECT_EQ(TupleHelper::getKey(tuple), "Key");
    EXPECT_EQ(TupleHelper::getOp(tuple), "SET");

    auto values = TupleHelper::getFieldsValues(tuple);
    EXPECT_EQ(values.size(), 1);
    EXPECT_EQ(TupleHelper::getField(values[0]), "Field");
    EXPECT_EQ(TupleHelper::getValue(values[0]), "Value");
}