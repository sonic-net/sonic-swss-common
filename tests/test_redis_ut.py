from swsscommon import swsscommon

def test_ProducerStateTable():
    db = swsscommon.DBConnector(0, "localhost", 6379, 0)
    ps = swsscommon.ProducerStateTable(db, "abc")
    cs = swsscommon.ConsumerStateTable(db, "abc")
    fvs = swsscommon.FieldValuePairs([('a','b')])
    ps.set("aaa", fvs)
    cfvs = swsscommon.FieldValuePairs([])
    (key, op) = cs.pop(cfvs)
    assert key == "aaa"
    assert op == "SET"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')

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
    cfvs = swsscommon.FieldValuePairs([])
    (key, op) = cst.pop(cfvs)
    assert key == "aaa"
    assert op == "SET"
    assert len(cfvs) == 1
    assert cfvs[0] == ('a', 'b')
