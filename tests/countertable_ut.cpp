#include "gtest/gtest.h"
#include "common/schema.h"
#include "common/countertable.h"

using namespace std;
using namespace swss;

#define COUNTER_TABLE   "COUNTERS"

static const vector<FieldValueTuple> port_name_map = {
    {"Ethernet0", "0x100000000001c"},
    {"Ethernet1", "0x100000000001d"}
};

static const vector<FieldValueTuple> gbport_name_map = {
    {"Ethernet0_system", "0x101010000000001"},
    {"Ethernet0_line",   "0x101010000000002"},
    {"Ethernet1_system", "0x101010000000003"},
    {"Ethernet1_line",   "0x101010000000004"}
};

static const vector<FieldValueTuple> port_stats = {
    {"SAI_PORT_STAT_IF_IN_ERRORS",      "1"},
    {"SAI_PORT_STAT_IF_OUT_ERRORS",     "1"},
    {"SAI_PORT_STAT_IF_IN_DISCARDS",    "1"},
    {"SAI_PORT_STAT_IF_OUT_DISCARDS",   "1"}
};

static void initCounterDB()
{
    DBConnector db("COUNTERS_DB", 0, true);
    Table table(&db, COUNTER_TABLE);
    for (auto kv: port_name_map)
    {
        db.hset(COUNTERS_PORT_NAME_MAP, kv.first, kv.second);
        table.set(kv.second, port_stats);
    }

    DBConnector gbdb("GB_COUNTERS_DB", 0, true);
    Table gbtable(&gbdb, COUNTER_TABLE);
    for (auto kv: gbport_name_map)
    {
        gbdb.hset(COUNTERS_PORT_NAME_MAP, kv.first, kv.second);
        gbtable.set(kv.second, port_stats);
    }
}

static void deinitCounterDB()
{
    DBConnector db("COUNTERS_DB", 0, true);
    Table table(&db, COUNTER_TABLE);
    for (auto kv: port_name_map)
    {
        table.del(kv.second);
    }
	db.del(COUNTERS_PORT_NAME_MAP);

    DBConnector gbdb("GB_COUNTERS_DB", 0, true);
    Table gbtable(&gbdb, COUNTER_TABLE);
    for (auto kv: gbport_name_map)
    {
        gbtable.del(kv.second);
    }
	gbdb.del(COUNTERS_PORT_NAME_MAP);
}

TEST(Counter, basic)
{
    initCounterDB();

    DBConnector db("COUNTERS_DB", 0, true);
    CounterTable counterTable(&db);

    bool ret;
    string port = "Ethernet0";
    string counterID = "SAI_PORT_STAT_IF_IN_ERRORS";
    string value;
    ret = counterTable.hget(PortCounter(), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "3");

    vector<FieldValueTuple>  values;
    ret = counterTable.get(PortCounter(), port, values);
    EXPECT_TRUE(ret);
    EXPECT_EQ(values.size(), port_stats.size());
    for (auto kv: values)
    {
        if (kv.first == "SAI_PORT_STAT_IF_IN_ERRORS" ||
            kv.first == "SAI_PORT_STAT_IF_IN_DISCARDS" ||
            kv.first == "SAI_PORT_STAT_IF_OUT_ERRORS" ||
            kv.first == "SAI_PORT_STAT_IF_OUT_DISCARDS")
        {
            EXPECT_EQ(kv.second, "3");
        }
    }

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::asic), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::systemside), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::lineside), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::lineside), port, "NONE_ID", value);
    EXPECT_FALSE(ret);

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::lineside), "abcd", counterID, value);
    EXPECT_FALSE(ret);

    deinitCounterDB();
}
