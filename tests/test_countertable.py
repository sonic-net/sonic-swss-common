import pytest
from swsscommon import swsscommon
from swsscommon.swsscommon import CounterTable, PortCounter, MacsecCounter

port_stats = swsscommon.FieldValuePairs([
    ("SAI_PORT_STAT_IF_IN_ERRORS",      "2"),
    ("SAI_PORT_STAT_IF_OUT_ERRORS",     "2"),
    ("SAI_PORT_STAT_IF_IN_DISCARDS",    "2"),
    ("SAI_PORT_STAT_IF_OUT_DISCARDS",   "2")])

@pytest.fixture(scope="module", autouse=True)
def setup_countersdb(request):
    db = swsscommon.DBConnector("COUNTERS_DB", 0, True)
    fvs = swsscommon.FieldValuePairs([
        ("Ethernet0", "0x100000000001a"),
        ("Ethernet1", "0x100000000001b")])
    for k, v in fvs:
        db.hset("COUNTERS_PORT_NAME_MAP", k, v)
        swsscommon.Table(db, "COUNTERS").set(v, port_stats)

    gbdb = swsscommon.DBConnector("GB_COUNTERS_DB", 0, True)
    fvs = swsscommon.FieldValuePairs([
        ("Ethernet0_system", "0x101010000000001"),
        ("Ethernet0_line",   "0x101010000000002"),
        ("Ethernet1_system", "0x101010000000003"),
        ("Ethernet1_line",   "0x101010000000004")])
    for k, v in fvs:
        gbdb.hset("COUNTERS_PORT_NAME_MAP", k, v)
        swsscommon.Table(gbdb, "COUNTERS").set(v, port_stats)

    yield

    gbdb.flushdb()
    db.flushdb()

def test_keycache():
    db = swsscommon.DBConnector("COUNTERS_DB", 0, True)
    gbdb = swsscommon.DBConnector("GB_COUNTERS_DB", 0, True)
    counterTable = CounterTable(db)

    cache = PortCounter().keyCacheInstance()
    assert not cache.enabled()
    assert cache.empty()

    cache.enable(counterTable)
    assert not cache.empty()
    assert cache.at("Ethernet0") ==  db.hget("COUNTERS_PORT_NAME_MAP", "Ethernet0")
    assert cache.at("Ethernet0_system") ==  gbdb.hget("COUNTERS_PORT_NAME_MAP", "Ethernet0_system")
    assert cache.at("Ethernet0_line") ==  gbdb.hget("COUNTERS_PORT_NAME_MAP", "Ethernet0_line")
    assert cache.at("Ethernetxx") == swsscommon.KeyCacheString.nullkey

    cache.disable()
    assert not cache.enabled()
    assert cache.empty()

def test_port():
    db = swsscommon.DBConnector("COUNTERS_DB", 0, True)
    counterTable = CounterTable(db)
    portName = "Ethernet0"
    counterID = 'SAI_PORT_STAT_IF_IN_ERRORS'

    r, value = counterTable.hget(PortCounter(), portName, counterID)
    assert r
    assert value == "6"
    r, values = counterTable.get(PortCounter(), portName)
    assert r
    assert len(values) == len(port_stats)

    asicPort = PortCounter(PortCounter.Mode_asic)
    r, value = counterTable.hget(asicPort, portName, counterID)
    assert r
    assert value == "2"

    systemsidePort = PortCounter(PortCounter.Mode_systemside)
    r, value = counterTable.hget(systemsidePort, portName, counterID)
    assert r
    assert value == "2"

    linesidePort = PortCounter(PortCounter.Mode_lineside)
    r, value = counterTable.hget(linesidePort, portName, counterID)
    assert r
    assert value == "2"

    # Enable key cache and test again
    cache = PortCounter().keyCacheInstance()
    cache.enable(counterTable)

    r, value = counterTable.hget(systemsidePort, portName, counterID)
    assert r
    assert value == "2"
    r, value = counterTable.hget(linesidePort, portName, counterID)
    assert r
    assert value == "2"

    cache.disable()
