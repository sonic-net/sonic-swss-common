from swsscommon import swsscommon

def test_is_interface_name_valid():
    invalid_interface_name = "TooLongInterfaceName"
    assert not swsscommon.isInterfaceNameValid(invalid_interface_name)

    validInterfaceName = "OkInterfaceName"
    assert swsscommon.isInterfaceNameValid(validInterfaceName)


def test_is_interface_name_valid_syntax():
    for name in (
        "Ethernet0",
        "PortChannel1",
        "PortChannel0001",
        "Eth0.100",
        "Ethernet0.100",
        "Ethernet-BP0",
        "Vrf-RED",
        "vtep1",
        "1Ethernet",
        "_eth0",
        ".eth0",
        "a" * 15,
    ):
        assert swsscommon.isInterfaceNameValid(name)

    for name in (
        "",
        ".",
        "..",
        "-Ethernet0",
        "Port Channel",
        "Ethernet0/1",
        "Ethernet0:1",
        "Ethernet0@1",
        "Ethernet;0",
        "Ethernet$0",
        "Ethernet`0",
        "Ethernet|0",
        "Ethernet&0",
        "Ethernet\\0",
        "Ethernet\n0",
        "Ethernet\t0",
        "a" * 16,
    ):
        assert not swsscommon.isInterfaceNameValid(name)
