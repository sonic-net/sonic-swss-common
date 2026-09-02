from swsscommon import swsscommon


def test_is_vrf_name_valid_syntax():
    for name in (
        "default",
        "mgmt",
        "vrfRED",
        "Vrf-RED_1",
        "Vrf.RED",
        "a" * 15,
    ):
        assert swsscommon.isVrfNameValid(name)

    for name in (
        "",
        ".",
        "..",
        "-VrfRED",
        "Vrf RED",
        "Vrf/RED",
        "Vrf:RED",
        "Vrf|RED",
        "a" * 16,
    ):
        assert not swsscommon.isVrfNameValid(name)
