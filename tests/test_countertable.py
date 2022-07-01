import pytest
from swsscommon import swsscommon
from swsscommon.swsscommon import CounterTable, PortCounter, MacsecCounter

port_stats = swsscommon.FieldValuePairs([
    ("SAI_PORT_STAT_IF_IN_ERRORS",      "2"),
    ("SAI_PORT_STAT_IF_OUT_ERRORS",     "2"),
    ("SAI_PORT_STAT_IF_IN_DISCARDS",    "2"),
    ("SAI_PORT_STAT_IF_OUT_DISCARDS",   "2")])

macsec_stats = swsscommon.FieldValuePairs([
    ("SAI_MACSEC_SA_STAT_OCTETS_PROTECTED", "1"),
    ("SAI_MACSEC_SA_STAT_OCTETS_ENCRYPTED", "1")])

macsec_name_map = swsscommon.FieldValuePairs([
    ("Ethernet112:5254007b4c480001:0", "oid:0x5c0000000006cd"),
    ("Ethernet112:1285af1bc5740001:0", "oid:0x5c0000000006ce")])

gbmacsec_name_map = swsscommon.FieldValuePairs([
    ("Ethernet116:5254007b4c480001:1", "oid:0x5c0000000006da"),
    ("Ethernet116:7228df195aa40001:1", "oid:0x5c0000000006d9")])

@pytest.fixture(scope="module", autouse=True)
def setup_countersdb(request):
    db = swsscommon.DBConnector("COUNTERS_DB", 0, True)
    table = swsscommon.Table(db, "COUNTERS")
    fvs = swsscommon.FieldValuePairs([
        ("Ethernet0", "0x100000000001a"),
        ("Ethernet1", "0x100000000001b"),
        ("Ethernet2", "0x100000000001c")])
    for k, v in fvs:
        db.hset("COUNTERS_PORT_NAME_MAP", k, v)
        table.set(v, port_stats)

    for k, v in macsec_name_map:
        db.hset("COUNTERS_MACSEC_NAME_MAP", k, v)
        table.set(v, macsec_stats)

    gbdb = swsscommon.DBConnector("GB_COUNTERS_DB", 0, True)
    gbtable = swsscommon.Table(gbdb, "COUNTERS")
    fvs = swsscommon.FieldValuePairs([
        ("Ethernet0_system", "0x101010000000001"),
        ("Ethernet0_line",   "0x101010000000002"),
        ("Ethernet1_system", "0x101010000000003"),
        ("Ethernet1_line",   "0x101010000000004")])
    for k, v in fvs:
        gbdb.hset("COUNTERS_PORT_NAME_MAP", k, v)
        gbtable.set(v, port_stats)

    for k, v in gbmacsec_name_map:
        gbdb.hset("COUNTERS_MACSEC_NAME_MAP", k, v)
        gbtable.set(v, macsec_stats)

    yield

    gbdb.flushdb()
    db.flushdb()

def test_keycache():
    db = swsscommon.DBConnector("COUNTERS_DB", 0, True)
    gbdb = swsscommon.DBConnector("GB_COUNTERS_DB", 0, True)
    counterTable = CounterTable(db)

    cache = PortCounter.keyCacheInstance()
    assert not cache.enabled()
    assert cache.empty()

    cache.enable(counterTable)
    assert not cache.empty()
    assert cache["Ethernet0"] == db.hget("COUNTERS_PORT_NAME_MAP", "Ethernet0")
    assert cache["Ethernet0_system"] == gbdb.hget("COUNTERS_PORT_NAME_MAP", "Ethernet0_system")
    assert cache.get("Ethernet0_line") == gbdb.hget("COUNTERS_PORT_NAME_MAP", "Ethernet0_line")
    assert not cache.get("Ethernetxx")

    tmpoid = "oid:tmptmptmp"
    db.hset("COUNTERS_PORT_NAME_MAP", "Ethernet100", tmpoid)
    cache.refresh(counterTable)
    assert cache.get("Ethernet100") == tmpoid

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

    asicPort = PortCounter(PortCounter.Mode_ASIC)
    r, value = counterTable.hget(asicPort, portName, counterID)
    assert r
    assert value == "2"

    systemsidePort = PortCounter(PortCounter.Mode_SYSTEMSIDE)
    r, value = counterTable.hget(systemsidePort, portName, counterID)
    assert r
    assert value == "2"

    linesidePort = PortCounter(PortCounter.Mode_LINESIDE)
    r, value = counterTable.hget(linesidePort, portName, counterID)
    assert r
    assert value == "2"

    # Enable key cache and test again
    cache = PortCounter.keyCacheInstance()
    cache.enable(counterTable)

    r, value = counterTable.hget(systemsidePort, portName, counterID)
    assert r
    assert value == "2"
    r, value = counterTable.hget(linesidePort, portName, counterID)
    assert r
    assert value == "2"

    # Port Ethernet2 is only on asic
    portName = "Ethernet2"
    r, value = counterTable.hget(PortCounter(), portName, counterID)
    assert r
    assert value == "2"
    r, values = counterTable.get(PortCounter(), portName)
    assert r
    assert len(values) == len(port_stats)

    r, value = counterTable.hget(asicPort, portName, counterID)
    assert r
    assert value == "2"
    r, values = counterTable.get(asicPort, portName)
    assert r
    assert len(values) == len(port_stats)

    r, value = counterTable.hget(systemsidePort, portName, counterID)
    assert not r
    assert not value
    r, values = counterTable.get(systemsidePort, portName)
    assert not r
    assert not values

    r, value = counterTable.hget(linesidePort, portName, counterID)
    assert not r
    assert not value
    r, values = counterTable.get(linesidePort, portName)
    assert not r
    assert not values

    cache.disable()

def test_macsec():
    db = swsscommon.DBConnector("COUNTERS_DB", 0, True)
    counterTable = CounterTable(db)
    macsecSA = macsec_name_map[0][0]
    counterID = macsec_stats[0][0]

    r, value = counterTable.hget(MacsecCounter(), macsecSA, counterID)
    assert r
    assert value == "1"
    r, values = counterTable.get(MacsecCounter(), macsecSA)
    assert r
    assert len(values) == len(macsec_stats)

    macsecSA = gbmacsec_name_map[0][0]
    r, value = counterTable.hget(MacsecCounter(), macsecSA, counterID)
    assert r
    assert value == "1"
    r, values = counterTable.get(MacsecCounter(), macsecSA)
    assert r
    assert len(values) == len(macsec_stats)

    # Enable key cache and test again
    cache = MacsecCounter.keyCacheInstance()
    cache.enable(counterTable)
    macsecSA = macsec_name_map[0][0]
    assert cache.get(macsecSA)[0] == db.getDbId()
    assert cache.get(macsecSA)[1] == db.hget("COUNTERS_MACSEC_NAME_MAP", macsecSA)
    assert not cache.get("nonono")

    r, value = counterTable.hget(MacsecCounter(), macsecSA, counterID)
    assert r
    assert value == "1"
    r, values = counterTable.get(MacsecCounter(), macsecSA)
    assert r
    assert len(values) == len(macsec_stats)

    cache.disable()
