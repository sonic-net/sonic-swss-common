#!/usr/bin/python3
"""Run a test with the selected, declared Trixie runtime dependencies."""

import os
from pathlib import Path, PurePosixPath
import posixpath
import signal
import shutil
import struct
import subprocess
import sys
import tarfile


def runtime_member_filter(member, destination):
    # Debian packages describe links relative to the installed filesystem.
    # Relocate absolute links into this staged root before applying Python's
    # confinement checks, so they cannot resolve against the execution host.
    if member.linkname.startswith("/") and (member.issym() or member.islnk()):
        target = posixpath.normpath("/" + member.linkname.lstrip("/")).lstrip("/")
        if member.issym():
            parent = PurePosixPath(member.name.lstrip("/")).parent
            target = posixpath.relpath(target, str(parent))
        member = member.replace(linkname=target)
    return tarfile.data_filter(member, destination)


def elf_interpreter(executable):
    """Read PT_INTERP from the System V ELF header, without a host tool."""
    with executable.open("rb") as stream:
        ident = stream.read(16)
        if ident[:4] != b"\x7fELF":
            return None
        endian = {1: "<", 2: ">"}[ident[5]]
        header_format, program_format, offset_index, size_index = {
            1: ("HHIIIIIHHHHHH", "IIIIIIII", 1, 4),
            2: ("HHIQQQIHHHHHH", "IIQQQQQQ", 2, 5),
        }[ident[4]]
        header = struct.unpack(endian + header_format, stream.read(struct.calcsize(endian + header_format)))
        for index in range(header[9]):
            stream.seek(header[4] + index * header[8])
            program = struct.unpack(endian + program_format, stream.read(struct.calcsize(endian + program_format)))
            if program[0] == 3:  # PT_INTERP
                stream.seek(program[offset_index])
                return os.fsdecode(stream.read(program[size_index]).rstrip(b"\0"))
    return None


def declared_runtime_command(command, runtime_root, patchelf):
    executable = Path(shutil.which(command[0]) or command[0])
    interpreter = elf_interpreter(executable)
    if interpreter is None:
        return command
    # Debian's merged-/usr package payload may omit the installed /lib64 alias.
    # Match the interpreter from the ELF against the declared package contents,
    # retaining one real file when both the multiarch path and aliases exist.
    loaders = {path.resolve() for path in runtime_root.rglob(Path(interpreter).name) if path.is_file()}
    if len(loaders) != 1:
        raise AssertionError(f"Expected one staged interpreter {interpreter}, found {sorted(loaders)}")
    loader = loaders.pop()
    if not loader.is_relative_to(runtime_root):
        raise AssertionError(f"Runtime interpreter escapes the staged root: {loader}")
    # Patch a private copy so self-exec (including the Go XML test wrapper)
    # selects the same declared interpreter on every execution.
    original = executable.resolve()
    staged = runtime_root.parent / "swsscommon-executable" / original.name
    staged.parent.mkdir()
    shutil.copy2(original, staged)
    staged.chmod(staged.stat().st_mode | 0o200)
    rpath = subprocess.check_output([patchelf, "--print-rpath", str(original)], text=True).strip()
    rpath = rpath.replace("${ORIGIN}", str(original.parent)).replace("$ORIGIN", str(original.parent))
    subprocess.run([patchelf, "--set-interpreter", str(loader), "--set-rpath", rpath, str(staged)], check=True)
    return [str(staged), *command[1:]]


def main():
    patchelf, archive, expected_library, yang_mode, *command = sys.argv[1:]
    if not command:
        raise ValueError("A test command is required")
    if yang_mode not in {"enabled", "disabled"}:
        raise ValueError(f"Unknown YANG mode: {yang_mode}")

    runtime_root = Path(os.environ["TEST_TMPDIR"]) / "swsscommon-runtime"
    runtime_root.mkdir()
    shutil.unpack_archive(archive, runtime_root, format="tar", filter=runtime_member_filter)

    runtime_root = runtime_root.resolve()
    library_dirs = set()
    for library in runtime_root.rglob("lib*.so*"):
        if not library.is_file():
            continue
        if not library.resolve().is_relative_to(runtime_root):
            raise AssertionError(f"Runtime library escapes the staged root: {library}")
        library_dirs.add(str(library.parent.resolve()))
    if not library_dirs:
        raise AssertionError("The staged runtime contains no shared libraries")

    environment = dict(os.environ)
    environment.update(
        LD_LIBRARY_PATH=":".join(sorted(library_dirs)),
        SWSS_RUNTIME_ROOT=str(runtime_root),
        SWSS_YANG_MODE=yang_mode,
    )
    # Source-built extensions can carry DT_RPATH entries pointing at system
    # directories; those precede LD_LIBRARY_PATH. Load the declared libraries
    # by exact path so the import test cannot pass against host copies.
    preload_names = ["libhiredis.so.1.1.0"]
    if yang_mode == "enabled":
        preload_names.append("libyang.so.3")
    preloads = []
    for name in preload_names:
        matches = {path.resolve() for path in runtime_root.rglob(name)}
        if len(matches) != 1:
            raise AssertionError(f"Expected one staged {name}, found {sorted(matches)}")
        preloads.append(str(matches.pop()))
    environment["LD_PRELOAD"] = ":".join(preloads)
    if expected_library != "-":
        library = Path(expected_library).resolve(strict=True)
        # The Go test is copied out of its runfiles so its original $ORIGIN
        # may no longer expose the producer. Stage only the declared library.
        producer_dir = runtime_root.parent / "swsscommon-library"
        producer_dir.mkdir()
        (producer_dir / library.name).symlink_to(library)
        environment["LD_LIBRARY_PATH"] = str(producer_dir) + ":" + environment["LD_LIBRARY_PATH"]
        environment["SWSS_EXPECTED_LIBRARY"] = str(library)

    result = subprocess.run(declared_runtime_command(command, runtime_root, patchelf), env=environment)
    if result.returncode < 0:
        child_signal = -result.returncode
        if child_signal not in {signal.SIGKILL, signal.SIGSTOP}:
            signal.signal(child_signal, signal.SIG_DFL)
        os.kill(os.getpid(), child_signal)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
