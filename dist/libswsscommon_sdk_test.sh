#!/usr/bin/env bash
# Check the installed headers and deployable ELF consumed by P4RT's Bazel 6.
set -euo pipefail

python3 - "$@" <<'PY'
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
import sys
import tarfile
import tempfile

sdk_path, runtime_path, architecture, yang_mode = sys.argv[1:5]
headers = [path for argument in sys.argv[5:] for path in shlex.split(argument)]
machine = {"amd64": "Advanced Micro Devices X86-64", "arm64": "AArch64"}[architecture]

with tarfile.open(sdk_path) as sdk:
    entries = {}
    for entry in sdk.getmembers():
        name = PurePosixPath(entry.name)
        assert not name.is_absolute() and ".." not in name.parts, entry.name
        if entry.isdir():
            continue
        assert str(name) not in entries, f"Duplicate SDK entry: {name}"
        entries[str(name)] = entry

    expected = {"lib/libswsscommon.so.0", "lib/libswsscommon.so"}
    for header in headers:
        source = Path(header)
        suffix = header.split("/common/", 1)[-1].removeprefix("common/")
        name = f"include/swss/{suffix}"
        expected.add(name)
        assert name in entries, f"Missing public header: {name}"
        assert entries[name].isfile(), f"Public header is not a file: {name}"
        assert sdk.extractfile(entries[name]).read() == source.read_bytes(), name

    assert set(entries) == expected, f"Unexpected SDK contents: {set(entries) ^ expected}"
    assert "include/swss/cfg_schema.h" in entries, "Missing selected schema header"
    assert "include/swss/c-api/dbconnector.h" in entries, "Missing public C API"
    library = entries["lib/libswsscommon.so.0"]
    assert library.isfile(), "SDK runtime library must be a real file"
    link = entries["lib/libswsscommon.so"]
    assert link.issym() and link.linkname == "libswsscommon.so.0", link
    library_bytes = sdk.extractfile(library).read()

with tarfile.open(runtime_path) as runtime:
    matches = [entry for entry in runtime.getmembers()
               if entry.name.endswith("/libswsscommon.so.0.0.0") and entry.isfile()]
    assert len(matches) == 1, "Expected one runtime Common library"
    assert runtime.extractfile(matches[0]).read() == library_bytes, (
        "SDK and runtime package must contain the same Common ELF"
    )

with tempfile.TemporaryDirectory(dir=os.environ.get("TEST_TMPDIR")) as directory:
    elf = Path(directory) / "libswsscommon.so.0"
    elf.write_bytes(library_bytes)
    env = dict(os.environ, LC_ALL="C")
    header = subprocess.check_output(["readelf", "--file-header", elf], text=True, env=env)
    dynamic = subprocess.check_output(["readelf", "--dynamic", elf], text=True, env=env)
    assert re.search(r"Machine:\s+" + re.escape(machine) + r"\s*$", header, re.M), header
    assert "Library soname: [libswsscommon.so.0]" in dynamic, dynamic
    assert "_solib_" not in dynamic and ".runfiles" not in dynamic, dynamic
    assert ("Shared library: [libyang.so.3]" in dynamic) == (yang_mode == "enabled"), dynamic

print(f"SDK headers, shared-library layout, {machine} ELF, and YANG {yang_mode} are valid")
print("SDK library bytes match the runtime package")
PY
