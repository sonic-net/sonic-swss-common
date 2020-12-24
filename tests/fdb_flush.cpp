#include "gtest/gtest.h"

#include "common/dbconnector.h"
#include "common/producertable.h"
#include "common/consumertable.h"
#include "common/redisapi.h"

#include <inttypes.h>
#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>

using namespace std;
using namespace swss;

static std::string tableName = "ASIC_STATE";

static void clear()
{
    DBConnector db("TEST_DB", 0, true);

    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);

    r.checkStatusOK();
}

static void insert(
        uint64_t switchId,
        uint64_t bvId,
        uint64_t portId,
        unsigned char mac,
        bool isStatic)
{
    DBConnector db("TEST_DB", 0, true);

    ProducerTable producer(&db, tableName);
    ConsumerTable consumer(&db, tableName);

    std::vector<FieldValueTuple> values;

    char buffer[4000];

    sprintf(buffer, "SAI_OBJECT_TYPE_FDB_ENTRY:{\"bvid\":\"oid:0x%" PRIx64 "\",\"mac\":\"00:00:00:00:00:%02X\",\"switch_id\":\"oid:0x%" PRIx64 "\"}", bvId, mac, switchId);

    std::string key = buffer;

    char port[1000];

    sprintf(port, "oid:0x%" PRIx64, portId);

    values.emplace_back("SAI_FDB_ENTRY_ATTR_PACKET_ACTION", "SAI_PACKET_ACTION_FORWARD");
    values.emplace_back("SAI_FDB_ENTRY_ATTR_TYPE", (isStatic ? "SAI_FDB_ENTRY_TYPE_STATIC" : "SAI_FDB_ENTRY_TYPE_DYNAMIC"));
    values.emplace_back("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID", port);

    producer.set(key, values, "set");

    std::string op;
    std::vector<FieldValueTuple> fvs;

    consumer.pop(key, op, fvs);
}

static size_t count()
{
    DBConnector db("TEST_DB", 0, true);

    auto keys = db.keys("ASIC_STATE:*");

    return keys.size();
}

void print()
{
    DBConnector db("TEST_DB", 0, true);

    auto keys = db.keys("ASIC_STATE:*");

    for (auto&k : keys)
    {
        printf("K %s\n", k.c_str());

        auto hash = db.hgetall(k);

        for (auto&h: hash)
        {
            printf(" * %s = %s\n", h.first.c_str(), h.second.c_str());
        }
    }
}

static std::string sOid(
        uint64_t oid)
{
    char buffer[100];

    sprintf(buffer, "oid:0x%" PRIx64, oid);

    return buffer;
}

static void populate()
{
    clear();

    insert(0x21000000000000, 0x26000000000001, 0x3a000000000001, 1, true);
    insert(0x21000000000000, 0x26000000000002, 0x3a000000000002, 2, false);

    insert(0x121000000000001, 0x126000000000003, 0x13a000000000003,  3, true);
    insert(0x121000000000001, 0x126000000000004, 0x13a000000000004,  4, false);
}

static void exec(
        uint64_t switchId,
        uint64_t bvId,
        uint64_t portId,
        std::string type)
{
    DBConnector db("TEST_DB", 0, true);

    populate();

    auto fdbFlushLuaScript = swss::readTextFile("./common/fdb_flush.v2.lua");

    auto sha = swss::loadRedisScript(&db, fdbFlushLuaScript);

    swss::RedisCommand command;

    command.format(
            "EVALSHA %s 4 %s %s %s %s",
            sha.c_str(),
            sOid(switchId).c_str(), // 0x0 == any
            sOid(bvId).c_str(), // 0x0 == any
            sOid(portId).c_str(), // 0x0 == any
            type.c_str()); //(0 ? "SAI_FDB_ENTRY_TYPE_STATIC" : "SAI_FDB_ENTRY_TYPE_DYNAMIC")); // empty == any

    swss::RedisReply r(&db, command);
}

static void mac(unsigned char m, bool is)
{
    DBConnector db("TEST_DB", 0, true);

    char buffer[100];

    sprintf(buffer, "*SAI_OBJECT_TYPE_FDB_ENTRY:*\"mac\":\"00:00:00:00:00:%02X\"*", m);

    auto keys = db.keys(buffer);

    if (is)
    {
        EXPECT_EQ(keys.size(), 1);
        return;
    }

    EXPECT_EQ(keys.size(), 0);
}

TEST(Fdb, flush)
{
    DBConnector db("TEST_DB", 0, true);

    exec(0,0,0, "");

    EXPECT_EQ(count(), 0);

    exec(0x21000000000000,0,0, "");

    mac(1,0);
    mac(2,0);
    mac(3,1);
    mac(4,1);

    exec(0x21000000000000,0x26000000000001,0, "");

    mac(1,0);
    mac(2,1);
    mac(3,1);
    mac(4,1);

    exec(0x21000000000000,0,0x3a000000000001, "");

    mac(1,0);
    mac(2,1);
    mac(3,1);
    mac(4,1);

    exec(0x21000000000000,0,0, "SAI_FDB_ENTRY_TYPE_STATIC");

    mac(1,0);
    mac(2,1);
    mac(3,1);
    mac(4,1);

    exec(0x21000000000000,0x26000000000001,0x3a000000000001, "SAI_FDB_ENTRY_TYPE_STATIC");

    mac(1,0);
    mac(2,1);
    mac(3,1);
    mac(4,1);

    exec(0x121000000000001,0,0, "");

    mac(1,1);
    mac(2,1);
    mac(3,0);
    mac(4,0);

    exec(0,0,0, "SAI_FDB_ENTRY_TYPE_STATIC");

    mac(1,0);
    mac(2,1);
    mac(3,0);
    mac(4,1);

    exec(0,0,0, "SAI_FDB_ENTRY_TYPE_DYNAMIC");

    mac(1,1);
    mac(2,0);
    mac(3,1);
    mac(4,0);
}
