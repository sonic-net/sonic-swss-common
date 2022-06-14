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

static const vector<FieldValueTuple> macsec_name_map = {
    {"Ethernet112:5254007b4c480001:0", "oid:0x5c0000000006cd"},
    {"Ethernet112:1285af1bc5740001:0", "oid:0x5c0000000006ce"}
};

static const vector<FieldValueTuple> gbmacsec_name_map = {
    {"Ethernet116:5254007b4c480001:1", "oid:0x5c0000000006da"},
    {"Ethernet116:7228df195aa40001:1", "oid:0x5c0000000006d9"}
};

static const vector<FieldValueTuple> macsec_stats = {
    {"SAI_MACSEC_SA_STAT_OCTETS_PROTECTED", "1"},
    {"SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", "1"}
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
    for (auto kv: macsec_name_map)
    {
        db.hset(COUNTERS_MACSEC_NAME_MAP, kv.first, kv.second);
        table.set(kv.second, macsec_stats);
    }

    DBConnector gbdb("GB_COUNTERS_DB", 0, true);
    Table gbtable(&gbdb, COUNTER_TABLE);
    for (auto kv: gbport_name_map)
    {
        gbdb.hset(COUNTERS_PORT_NAME_MAP, kv.first, kv.second);
        gbtable.set(kv.second, port_stats);
    }
    for (auto kv: gbmacsec_name_map)
    {
        gbdb.hset(COUNTERS_MACSEC_NAME_MAP, kv.first, kv.second);
        gbtable.set(kv.second, macsec_stats);
    }
}

static void deinitCounterDB()
{
    DBConnector db("COUNTERS_DB", 0, true);
    db.flushdb();

    DBConnector gbdb("GB_COUNTERS_DB", 0, true);
    gbdb.flushdb();
}

static void testPort(DBConnector &db, CounterTable &counterTable)
{
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
    ret = counterTable.hget(PortCounter(PortCounter::Mode::ASIC), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::SYSTEMSIDE), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::LINESIDE), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::LINESIDE), port, "NONE_ID", value);
    EXPECT_FALSE(ret);

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::LINESIDE), "abcd", counterID, value);
    EXPECT_FALSE(ret);

    // Enable key cache
    KeyCache<string> &cache = PortCounter::keyCacheInstance();
    cache.enable(counterTable);

    value.clear();
    ret = counterTable.hget(PortCounter(), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "3");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::ASIC), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::SYSTEMSIDE), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    value.clear();
    ret = counterTable.hget(PortCounter(PortCounter::Mode::LINESIDE), port, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    cache.disable();
}

static void testMacsec(DBConnector &db, CounterTable &counterTable)
{
    bool ret;
    string macsecSA = macsec_name_map[0].first;
    string counterID = macsec_stats[0].first;

    string value;
    ret = counterTable.hget(MacsecCounter(), macsecSA, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    vector<FieldValueTuple>  values;
    ret = counterTable.get(MacsecCounter(), macsecSA, values);
    EXPECT_TRUE(ret);
    EXPECT_EQ(values.size(), macsec_stats.size());

    macsecSA = gbmacsec_name_map[0].first;
    value.clear();
    ret = counterTable.hget(MacsecCounter(), macsecSA, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    values.clear();
    ret = counterTable.get(MacsecCounter(), macsecSA, values);
    EXPECT_TRUE(ret);
    EXPECT_EQ(values.size(), macsec_stats.size());

    // Enable key cache
    KeyCache<Counter::KeyPair> &cache = MacsecCounter::keyCacheInstance();
    cache.enable(counterTable);

    value.clear();
    ret = counterTable.hget(MacsecCounter(), macsecSA, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    macsecSA = macsec_name_map[0].first;
    value.clear();
    ret = counterTable.hget(MacsecCounter(), macsecSA, counterID, value);
    EXPECT_TRUE(ret);
    EXPECT_EQ(value, "1");

    cache.disable();
}

TEST(Counter, basic)
{
    initCounterDB();

    DBConnector db("COUNTERS_DB", 0, true);
    CounterTable counterTable(&db);

    testPort(db, counterTable);
    testMacsec(db, counterTable);

    deinitCounterDB();
}
