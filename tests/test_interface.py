from swsscommon import swsscommon
from swsscommon.swsscommon import  validate_interface_name_length, iface_name_max_length

IFNAMSIZ = 16

def test_validate_interface_name_length():
    invalid_interface_name = "TooLongInterfaceName"
    assert not validate_interface_name_length(invalid_interface_name)

    validInterfaceName = "OkInterfaceName"
    assert validate_interface_name_length(validInterfaceName)

def test_iface_name_max_length():
    assert iface_name_max_length() == IFNAMSIZ
