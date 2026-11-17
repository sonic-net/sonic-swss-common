import ctypes as C
import os
from pathlib import Path
import subprocess

import pytest

pytest_plugins = ["test_redis_authentication", "test_configdb_authentication"]


class Result(C.Structure):
    _fields_ = [("exception", C.c_int), ("message", C.c_void_p), ("location", C.c_void_p)]


@pytest.fixture
def api():
    lib = C.CDLL("libswsscommon.so")
    pointer = C.POINTER(C.c_void_p)
    signatures = {
        "SWSSDBConnector_new_named_with_auth": [C.c_char_p, C.c_uint32, C.c_uint8, C.c_uint8, pointer],
        "SWSSDBConnector_new_keyed_with_auth": [C.c_char_p, C.c_uint32, C.c_uint8, C.c_char_p, C.c_char_p, C.c_uint8, pointer],
        "SWSSDBConnector_free": [C.c_void_p],
        "SWSSDBConnector_set": [C.c_void_p, C.c_char_p, C.c_void_p],
        "SWSSSonicV2Connector_new_with_auth": [C.c_uint8, C.c_char_p, C.c_uint8, pointer],
        "SWSSSonicV2Connector_connect": [C.c_void_p, C.c_char_p, C.c_uint8],
        "SWSSSonicV2Connector_get_redis_client": [C.c_void_p, C.c_char_p, pointer],
        "SWSSSonicV2Connector_free": [C.c_void_p],
        "SWSSConfigDBConnector_new_with_auth": [C.c_uint8, C.c_char_p, C.c_uint8, pointer],
        "SWSSConfigDBConnector_connect": [C.c_void_p, C.c_uint8, C.c_uint8],
        "SWSSConfigDBConnector_get_redis_client": [C.c_void_p, C.c_char_p, pointer],
        "SWSSConfigDBConnector_free": [C.c_void_p],
    }
    for name, arguments in signatures.items():
        getattr(lib, name).argtypes = arguments
        getattr(lib, name).restype = Result
    lib.SWSSString_free.argtypes = [C.c_void_p]
    lib.SWSSString_free.restype = None
    lib.SWSSString_new_c_str.argtypes = [C.c_char_p]
    lib.SWSSString_new_c_str.restype = C.c_void_p
    return lib


def check(api, result, expected=0):
    api.SWSSString_free(result.message)
    api.SWSSString_free(result.location)
    assert result.exception == expected


def write(api, db, expected):
    value = api.SWSSString_new_c_str(b"value")
    try:
        check(api, api.SWSSDBConnector_set(db, b"c-api", value), expected)
    finally:
        api.SWSSString_free(value)


@pytest.mark.parametrize("keyed", [False, True])
@pytest.mark.parametrize("authenticate", [False, True])
def test_c_dbconnector_option(protected, api, keyed, authenticate):
    db = C.c_void_p()
    if keyed:
        result = api.SWSSDBConnector_new_keyed_with_auth(b"STATE_DB", 500, 0, b"", b"", authenticate, C.byref(db))
    else:
        result = api.SWSSDBConnector_new_named_with_auth(b"STATE_DB", 500, 0, authenticate, C.byref(db))
    check(api, result)
    try:
        write(api, db, 0 if authenticate else 1)
    finally:
        check(api, api.SWSSDBConnector_free(db))


@pytest.mark.parametrize("authenticate", [False, True])
def test_c_sonicv2_option(protected, api, authenticate):
    connector, db = C.c_void_p(), C.c_void_p()
    check(api, api.SWSSSonicV2Connector_new_with_auth(1, None, authenticate, C.byref(connector)))
    try:
        check(api, api.SWSSSonicV2Connector_connect(connector, b"STATE_DB", 0))
        check(api, api.SWSSSonicV2Connector_get_redis_client(connector, b"STATE_DB", C.byref(db)))
        write(api, db, 0 if authenticate else 1)
    finally:
        check(api, api.SWSSSonicV2Connector_free(connector))


@pytest.mark.parametrize("authenticate", [False, True])
def test_c_configdb_option(config_metadata, api, authenticate):
    connector, db = C.c_void_p(), C.c_void_p()
    check(api, api.SWSSConfigDBConnector_new_with_auth(1, None, authenticate, C.byref(connector)))
    try:
        check(api, api.SWSSConfigDBConnector_connect(connector, 0, 0))
        check(api, api.SWSSConfigDBConnector_get_redis_client(connector, b"CONFIG_DB", C.byref(db)))
        write(api, db, 0 if authenticate else 1)
    finally:
        check(api, api.SWSSConfigDBConnector_free(connector))


def test_c_rejected_credentials(protected, api):
    protected["secret"].write_text("wrong-test-password")
    output = C.c_void_p()
    check(api, api.SWSSDBConnector_new_named_with_auth(b"STATE_DB", 500, 0, 1, C.byref(output)), 1)


def test_rust_authentication(config_metadata):
    target = Path(os.environ.get('CARGO_TARGET_DIR', 'target'))
    binaries = [p for p in (target / 'debug/deps').glob('authentication-*')
                if p.is_file() and os.access(p, os.X_OK)]
    assert binaries, 'Run cargo test --workspace --all-features before this suite'
    binary = max(binaries, key=lambda p: p.stat().st_mtime)
    subprocess.run([str(binary.resolve()), '--ignored', '--nocapture'],
        env={**os.environ, 'SWSS_AUTH_DB_CONFIG': str(config_metadata['metadata'])},
        check=True, timeout=10)
