import time
from swsscommon import swsscommon

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
