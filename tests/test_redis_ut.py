from swsscommon import swsscommon

def test_ProducerStateTable():
    db = swsscommon.DBConnector(0, "localhost", 6379, 0)
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
    db = swsscommon.DBConnector(0, "localhost", 6379, 0)
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
    db = swsscommon.DBConnector(0, "localhost", 6379, 0)
    t = swsscommon.Table(db, "testsst", '|')
    sel = swsscommon.Select()
    cst = swsscommon.SubscriberStateTable(db, "testsst")
    sel.addSelectable(cst)
    fvs = swsscommon.FieldValuePairs([('a','b')])
    t.set("aaa", fvs)
    (state, c, fd) = sel.select()
    assert state == swsscommon.Select.OBJECT
    (key, op, cfvs) = cst.pop()
    assert key == "aaa"
    assert op == "SET"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')
