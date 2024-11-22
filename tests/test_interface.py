from swsscommon import swsscommon

def test_is_interface_name_valid():
    invalid_interface_name = "TooLongInterfaceName"
    assert not swsscommon.isInterfaceNameValid(invalid_interface_name)

    validInterfaceName = "OkInterfaceName"
    assert swsscommon.isInterfaceNameValid(validInterfaceName)
