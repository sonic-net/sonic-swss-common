#!/usr/bin/python3
"""Check the installed Python archive exposes the selected native API."""

import os
import re
import struct
import zlib
from pathlib import Path
import subprocess
import sys
import shutil


_CHECK_IMPORT = r"""
import ctypes
import os
from pathlib import Path

import swsscommon
from swsscommon import _swsscommon as native
from swsscommon import swsscommon as binding

package_root = Path(os.environ["SWSS_PACKAGE_ROOT"]).resolve()
for module in (swsscommon, binding, native):
    assert Path(module.__file__).resolve().is_relative_to(package_root), module.__file__

# The redis-py adapter calls this exported SWIG function. An invalid path
# exercises the native exception mapping without contacting a Redis server.
try:
    binding.openRedisUnixSocket("relative.sock", 1, False)
except ValueError as error:
    assert "absolute Redis Unix path" in str(error), error
else:
    raise AssertionError("Relative Redis socket path should have been rejected")

runtime_root = Path(os.environ["SWSS_RUNTIME_ROOT"]).resolve()
loaded = set()
for line in Path("/proc/self/maps").read_text().splitlines():
    fields = line.split(maxsplit=5)
    if len(fields) == 6 and fields[5].startswith("/"):
        loaded.add(Path(fields[5]).resolve())

def check_runtime_library(prefix, required):
    matches = {path for path in loaded if path.name.startswith(prefix)}
    if not required:
        assert not matches, matches
        return
    assert len(matches) == 1, matches
    library = matches.pop()
    assert library.is_relative_to(runtime_root), library

check_runtime_library("libswsscommon.so.", True)
check_runtime_library("libhiredis.so.", True)
check_runtime_library("libyang.so.", os.environ["SWSS_YANG_MODE"] == "enabled")

# The native authentication API reads the same singleton that Python initializes.
class Result(ctypes.Structure):
    _fields_ = [("exception", ctypes.c_int), ("message", ctypes.c_void_p), ("location", ctypes.c_void_p)]

common = ctypes.CDLL("libswsscommon.so.0")
common.SWSSSonicDBConfig_initialize.argtypes = [ctypes.c_char_p]
common.SWSSSonicDBConfig_initialize.restype = Result
config = runtime_root / "run/redis/sonic-db/database_config.json"
assert common.SWSSSonicDBConfig_initialize(os.fsencode(config)).exception == 0
assert binding.SonicDBConfig.isInit(), "Python and the C API must share the database configuration"

yang_classes = (
    "DefaultValueProvider",
    "DecoratorTable",
    "DecoratorSubscriberStateTable",
)
yang_constants = {
    "CFG_ACL_TABLE_TABLE_NAME": "ACL_TABLE",
    "CFG_PORT_TABLE_NAME": "PORT",
}
if os.environ["SWSS_YANG_MODE"] == "enabled":
    for name in yang_classes:
        assert hasattr(binding, name), name
    for name, value in yang_constants.items():
        assert getattr(binding, name) == value, name
    # Construction crosses the generated SWIG wrapper into the enabled native
    # class. The C++ fixture test covers its model evaluation behavior.
    provider = binding.DefaultValueProvider()
    assert provider.thisown
else:
    for name in (*yang_classes, *yang_constants):
        assert not hasattr(binding, name), name

print(f"Imported packaged Python bindings with YANG {os.environ['SWSS_YANG_MODE']}")
Path(os.environ["SWSS_RESULT_FILE"]).write_text("passed\n")
"""


def check_debug_symbols(install_root, symbols_archive):
    symbols_root = Path(os.environ["TEST_TMPDIR"]) / "swsscommon-symbols"
    shutil.unpack_archive(symbols_archive, symbols_root, format="tar", filter="data")
    package = install_root / "usr/lib/python3/dist-packages/swsscommon"
    assert {path.name for path in package.iterdir()} == {
        "__init__.py", "redis.py", "swsscommon.py", "_swsscommon.so", "lib_swsscommon.so",
    }
    assert (package / "_swsscommon.so").readlink() == Path("lib_swsscommon.so")
    binary = package / "lib_swsscommon.so"
    notes = subprocess.check_output(["readelf", "-n", binary], text=True)
    build_id = re.search(r"Build ID: ([0-9a-f]+)", notes).group(1)
    debug_file = symbols_root / "usr/lib/debug/.build-id" / build_id[:2] / (build_id[2:] + ".debug")
    assert debug_file.is_file(), debug_file
    debug_notes = subprocess.check_output(["readelf", "-n", debug_file], text=True)
    assert "Build ID: " + build_id in debug_notes
    sections = subprocess.check_output(["readelf", "-SW", binary], text=True)
    assert not re.search(r"\.(?:z?debug_)\w+", sections), sections
    debug_sections = subprocess.check_output(["readelf", "-SW", debug_file], text=True)
    assert ".debug_info" in debug_sections and ".debug_line" in debug_sections
    assert not (install_root / "usr/lib/debug").exists()
    debuglink = symbols_root / "debuglink"
    subprocess.run(["objcopy", "--dump-section", ".gnu_debuglink=" + str(debuglink),
                    binary, symbols_root / "runtime-copy"], check=True)
    data = debuglink.read_bytes()
    end = data.index(b"\0")
    assert data[:end].decode() == debug_file.name
    assert struct.unpack_from("<I", data, (end + 4) & ~3)[0] == zlib.crc32(debug_file.read_bytes())
    commands = ["gdb", "--batch", "--nx", "--quiet", "-ex", "set debuginfod enabled off",
                "-ex", "set debug-file-directory " + str(symbols_root / "usr/lib/debug"),
                "-ex", "file " + str(binary), "-ex", "info line PyInit__swsscommon"]
    output = subprocess.check_output(commands, text=True, stderr=subprocess.STDOUT)
    assert re.search(r'Line [0-9]+ of ".*swsscommon_wrap\.cpp"', output), output
    print("Matched Python extension symbols and GDB source line:", build_id)


def main():
    runner, patchelf, runtime_archive, archive, symbols_archive, yang_mode = sys.argv[1:]
    if yang_mode not in {"enabled", "disabled"}:
        raise ValueError(f"Unknown YANG mode: {yang_mode}")

    install_root = Path(os.environ["TEST_TMPDIR"]) / "swsscommon-package"
    shutil.unpack_archive(archive, install_root, format="tar", filter="data")

    check_debug_symbols(install_root, symbols_archive)

    package_root = install_root / "usr/lib/python3/dist-packages"
    result_file = install_root / "import-check-passed"
    result_file.unlink(missing_ok=True)
    environment = dict(os.environ)
    environment.update(
        PYTHONNOUSERSITE="1",
        PYTHONPATH=str(package_root),
        RUNFILES_DIR="",
        RUNFILES_MANIFEST_FILE="",
        SWSS_PACKAGE_ROOT=str(package_root),
        SWSS_RESULT_FILE=str(result_file),
        SWSS_YANG_MODE=yang_mode,
    )
    subprocess.run(
        [runner, patchelf, runtime_archive, "-", yang_mode, sys.executable, "-S", "-c", _CHECK_IMPORT],
        env=environment,
        check=True,
    )
    # Require positive evidence that all installed import checks completed.
    if not result_file.is_file() or result_file.read_text() != "passed\n":
        raise AssertionError("Packaged Python import checks did not complete")


if __name__ == "__main__":
    main()
