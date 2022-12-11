import os
import pytest
from swsscommon import swsscommon

@pytest.fixture
def prepare_yang_module():
    # copy yang module
    yang_file_name = "sonic-interface.yang"
    yang_module_path = "/usr/local/yang-models"
    test_dir = os.path.dirname(os.path.realpath(__file__))
    yang_module_src = os.path.join(test_dir, "yang", yang_file_name)
    yang_module_dest = os.path.join(yang_module_path, yang_file_name)
    os.system("sudo mkdir {}".format(yang_module_path))
    os.system("sudo cp {} {}".format(yang_module_src, yang_module_dest))

    yield

    # cleanup yang module
    os.system("sudo rm -rf {}".format(yang_module_path))

@pytest.fixture
def reset_database():
    os.system("redis-cli FLUSHALL")

    yield

    os.system("redis-cli FLUSHALL")

def check_read_api_result(decorator, table, key, field, expected):
    entry = decorator.get_entry(table, key)
    assert entry[field] == expected

    table_data = decorator.get_table(table)
    assert table_data[key][field] == expected

    config = decorator.get_config()
    assert config[table][key][field] == expected

def check_table_read_api_result(table, key, fieldname, expected):
    # check get
    result, fields = table.get(key)
    assert result
    field_exist = False
    for field in fields:
        if field[0] == fieldname:
            field_exist = True
            assert field[1] == expected
    assert field_exist
    
    # check hget
    result, value = table.hget(key, fieldname)
    assert result
    assert value == expected
    
    # check getKeys
    keys = table.getKeys()
    assert key in keys

def test_decorator_read_yang_default_value(prepare_yang_module, reset_database):
    """
    Test YangDefaultDecorator read correct default values from Yang model.
    """
    conn = swsscommon.ConfigDBConnector()
    decorator = swsscommon.YangDefaultDecorator(conn)
    decorator.connect(wait_for_init=False)

    # set to table without default value
    table = "INTERFACE"
    key = "TEST_INTERFACE"
    data = { "test_field": "test_value" }
    field = "nat_zone"
    decorator.set_entry(table, key, data)

    # check read API
    check_read_api_result(decorator, table, key, field, "0")

    # test set_entry
    data[field] = '1'
    decorator.set_entry(table, key, data)

    # check read API
    check_read_api_result(decorator, table, key, field, "1")

    # test mod_entry
    data[field] = '2'
    decorator.mod_entry(table, key, data)

    # check read API
    check_read_api_result(decorator, table, key, field, "2")

    # test mod_config
    config = decorator.get_config()
    config[table][key][field] = "3"
    decorator.mod_config(config)

    # check read API
    check_read_api_result(decorator, table, key, field, "3")

def test_table_read_yang_default_value(prepare_yang_module, reset_database):
    """
    Test DecoratorTable read correct default values from Yang model.
    """
    # set to table without default value
    conn = swsscommon.ConfigDBConnector()
    decorator = swsscommon.YangDefaultDecorator(conn)
    decorator.connect(wait_for_init=False)
    table = "INTERFACE"
    key = "TEST_INTERFACE"
    data = { "test_field": "test_value" }
    fieldname = "nat_zone"
    decorator.set_entry(table, key, data)

    # check read API
    db = swsscommon.DBConnector("CONFIG_DB", 0)
    table = swsscommon.DecoratorTable(db, 'INTERFACE')

    # check read API
    check_table_read_api_result(table, key, fieldname, "0")

def test_DecoratorSubscriberStateTable(prepare_yang_module, reset_database):
    """
    Test DecoratorSubscriberStateTable can decorate default value.
    """
    tablename = "INTERFACE"
    profile_key = "TEST_INTERFACE"
    
    # setup select
    db = swsscommon.DBConnector("CONFIG_DB", 0, True)
    db.flushdb()
    table = swsscommon.DecoratorTable(db, tablename)
    sel = swsscommon.Select()
    cst = swsscommon.DecoratorSubscriberStateTable(db, tablename)
    sel.addSelectable(cst)

    # set profile
    fvs = swsscommon.FieldValuePairs([('a','b')])
    table.set(profile_key, fvs, "", "", 100)

    # check select event
    (state, c) = sel.select()
    assert state == swsscommon.Select.OBJECT

    entries = swsscommon.transpose_pops(cst.pops())
    entry_exist = False
    for entry in entries:
        (key, op, cfvs) = entry
        if key == profile_key:
            entry_exist = True
            assert op == "SET"
            assert len(cfvs) == 2
            assert cfvs[0] == ('a', 'b')
            assert cfvs[1] == ('nat_zone', '0')
    assert entry_exist

    # overwrite default value
    fvs = swsscommon.FieldValuePairs([('a','b'), ('nat_zone','1')])
    table.set(profile_key, fvs, "", "", 100)

    # check select event
    (state, c) = sel.select()
    assert state == swsscommon.Select.OBJECT

    print("start check")
    entries = swsscommon.transpose_pops(cst.pops())
    print(entries)
    entry_exist = False
    for entry in entries:
        (key, op, cfvs) = entry
        if key == profile_key:
            entry_exist = True
            assert op == "SET"
            assert len(cfvs) == 2
            assert cfvs[0] == ('a', 'b')
            assert cfvs[1] == ('nat_zone', '1')
    assert entry_exist
    