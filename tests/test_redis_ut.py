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
