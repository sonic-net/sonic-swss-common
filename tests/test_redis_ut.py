import time
import pytest
from threading import Thread
from pympler.tracker import SummaryTracker
from swsscommon import swsscommon
from swsscommon.swsscommon import DBInterface, SonicV2Connector, SonicDBConfig

existing_file = "./tests/redis_multi_db_ut_config/database_config.json"

@pytest.fixture(scope="session", autouse=True)
def prepare(request):
    SonicDBConfig.initialize(existing_file)

def test_ProducerTable():
    db = swsscommon.DBConnector("APPL_DB", 0, True)
    ps = swsscommon.ProducerTable(db, "abc")
    cs = swsscommon.ConsumerTable(db, "abc")
    fvs = swsscommon.FieldValuePairs([('a','b')])
    ps.set("bbb", fvs)
    (key, op, cfvs) = cs.pop()
    assert key == "bbb"
    assert op == "SET"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')

def test_ProducerStateTable():
    db = swsscommon.DBConnector("APPL_DB", 0, True)
    ps = swsscommon.ProducerStateTable(db, "abc")
    cs = swsscommon.ConsumerStateTable(db, "abc")
    fvs = swsscommon.FieldValuePairs([('a','b')])
    ps.set("aaa", fvs)
    (key, op, cfvs) = cs.pop()
    assert key == "aaa"
    assert op == "SET"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')

def test_Table():
    db = swsscommon.DBConnector("APPL_DB", 0, True)
    tbl = swsscommon.Table(db, "test_TABLE")
    fvs = swsscommon.FieldValuePairs([('a','b'), ('c', 'd')])
    tbl.set("aaa", fvs)
    keys = tbl.getKeys()
    assert len(keys) == 1
    assert keys[0] == "aaa"
    (status, fvs) = tbl.get("aaa")
    assert status == True
    assert len(fvs) == 2
    assert fvs[0] == ('a', 'b')
    assert fvs[1] == ('c', 'd')

def test_SubscriberStateTable():
    db = swsscommon.DBConnector("APPL_DB", 0, True)
    t = swsscommon.Table(db, "testsst")
    sel = swsscommon.Select()
    cst = swsscommon.SubscriberStateTable(db, "testsst")
    sel.addSelectable(cst)
    fvs = swsscommon.FieldValuePairs([('a','b')])
    t.set("aaa", fvs)
    (state, c) = sel.select()
    assert state == swsscommon.Select.OBJECT
    (key, op, cfvs) = cst.pop()
    assert key == "aaa"
    assert op == "SET"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')

def test_Notification():
    db = swsscommon.DBConnector("APPL_DB", 0, True)
    ntfc = swsscommon.NotificationConsumer(db, "testntf")
    sel = swsscommon.Select()
    sel.addSelectable(ntfc)
    fvs = swsscommon.FieldValuePairs([('a','b')])
    ntfp = swsscommon.NotificationProducer(db, "testntf")
    ntfp.send("aaa", "bbb", fvs)
    (state, c) = sel.select()
    assert state == swsscommon.Select.OBJECT
    (op, data, cfvs) = ntfc.pop()
    assert op == "aaa"
    assert data == "bbb"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')

def test_DBConnectorRedisClientName():
    db = swsscommon.DBConnector("APPL_DB", 0, True)
    time.sleep(1)
    assert db.getClientName() == ""
    client_name = "foo"
    db.setClientName(client_name)
    time.sleep(1)
    assert db.getClientName() == client_name
    client_name = "bar"
    db.setClientName(client_name)
    time.sleep(1)
    assert db.getClientName() == client_name
    client_name = "foobar"
    db.setClientName(client_name)
    time.sleep(1)
    assert db.getClientName() == client_name


def test_SelectMemoryLeak():
    N = 50000
    def table_set(t, state):
        fvs = swsscommon.FieldValuePairs([("status", state)])
        t.set("123", fvs)

    def generator_SelectMemoryLeak():
        app_db = swsscommon.DBConnector("APPL_DB", 0, True)
        t = swsscommon.Table(app_db, "TABLE")
        for i in range(int(N/2)):
            table_set(t, "up")
            table_set(t, "down")

    tracker = SummaryTracker()
    appl_db = swsscommon.DBConnector("APPL_DB", 0, True)
    sel = swsscommon.Select()
    sst = swsscommon.SubscriberStateTable(appl_db, "TABLE")
    sel.addSelectable(sst)
    thr = Thread(target=generator_SelectMemoryLeak)
    thr.daemon = True
    thr.start()
    time.sleep(5)
    for _ in range(N):
        state, c = sel.select(1000)
    diff = tracker.diff()
    cases = []
    for name, count, _ in diff:
        if count >= N:
            cases.append("%s - %d objects for %d repeats" % (name, count, N))
    thr.join()
    assert not cases


def test_DBInterface():
    dbintf = DBInterface()
    dbintf.set_redis_kwargs("", "127.0.0.1", 6379)
    dbintf.connect(15, "TEST_DB")

    db = SonicV2Connector(use_unix_socket_path=True, namespace='')
    assert db.TEST_DB == 'TEST_DB'
    assert db.namespace == ''
    db.connect("TEST_DB")
    db.set("TEST_DB", "key0", "field1", "value2")
    fvs = db.get_all("TEST_DB", "key0")
    assert "field1" in fvs
    assert fvs["field1"] == "value2"

    # Test del
    db.set("TEST_DB", "key3", "field4", "value5")
    deleted = db.delete("TEST_DB", "key3")
    assert deleted == 1
    deleted = db.delete("TEST_DB", "key3")
    assert deleted == 0

    # Test pubsub
    redisclient = db.get_redis_client("TEST_DB")
    ps = redisclient.pubsub()

    # Test dict.get()
    assert fvs.get("field1") == "value2"
    assert fvs.get("field1_noexisting") == None
    assert fvs.get("field1", "default") == "value2"
    assert fvs.get("nonfield", "default") == "default"

    # Test dict.update()
    other = { "field1": "value3", "field4": "value4" }
    fvs.update(other)
    assert len(fvs) == 2
    assert fvs["field1"] == "value3"
    assert fvs["field4"] == "value4"
    # Test dict.update() accepts no arguments, and then no update happens
    fvs.update()
    assert len(fvs) == 2
    assert fvs["field1"] == "value3"
    assert fvs["field4"] == "value4"
    fvs.update(field5='value5', field6='value6')
    assert fvs["field5"] == "value5"
    with pytest.raises(TypeError):
        fvs.update(fvs, fvs)

    # Test blocking
    fvs = db.get_all("TEST_DB", "key0", blocking=True)
    assert "field1" in fvs
    assert fvs["field1"] == "value2"
    assert fvs.get("field1", "default") == "value2"
    assert fvs.get("nonfield", "default") == "default"

    # Test empty/none namespace
    db = SonicV2Connector(use_unix_socket_path=True, namespace=None)
    assert db.namespace == ''

    # Test default namespace parameter
    db = SonicV2Connector(use_unix_socket_path=True)
    assert db.namespace == ''

    # Test no exception
    try:
        db = SonicV2Connector(host='127.0.0.1')
        db = SonicV2Connector(use_unix_socket_path=True, namespace='', decode_responses=True)
        db = SonicV2Connector(use_unix_socket_path=False, decode_responses=True)
        db = SonicV2Connector(host="127.0.0.1", decode_responses=True)
    except:
        assert False, 'Unexpected exception raised'

    # Test exception
    with pytest.raises(ValueError):
        db = SonicV2Connector(decode_responses=False)
