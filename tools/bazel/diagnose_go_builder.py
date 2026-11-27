#!/usr/bin/env python3
"""Retain bounded, allowlisted evidence after a Go builder action fails."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import stat
import subprocess


MAX_BUILDERS = 16
MAX_DIRECTORIES = 100000


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inspect_builder(path):
    record = {"path": str(path)}
    try:
        metadata = path.lstat()
        record["mode"] = oct(stat.S_IMODE(metadata.st_mode))
        record["is_symlink"] = path.is_symlink()
        if path.is_symlink():
            record["link_target"] = os.readlink(path)
        target = path.resolve(strict=True)
        record["resolved_target"] = str(target)
        record["target_mode"] = oct(stat.S_IMODE(target.stat().st_mode))
        record["size"] = target.stat().st_size
        if target.is_file() and record["size"] <= 128 * 1024 * 1024:
            record["sha256"] = sha256(target)
            result = subprocess.run(["readelf", "-l", str(target)],
                                    capture_output=True, text=True, timeout=10)
            record["readelf_returncode"] = result.returncode
            record["elf_interpreter"] = [line.strip() for line in result.stdout.splitlines()
                                         if "Requesting program interpreter:" in line]
            record["readelf_error"] = result.stderr[:4096]
            record["interpreter_paths"] = []
            for line in record["elf_interpreter"]:
                interpreter = Path(line.split("Requesting program interpreter: ", 1)[1].rstrip("]"))
                record["interpreter_paths"].append({"path": str(interpreter),
                    "exists": interpreter.exists(), "resolved": str(interpreter.resolve())})
        else:
            record["inspection_skipped"] = "Not a regular file within the 128 MiB limit"
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        record["error"] = str(error)
    return record


def collect(output_base, destination):
    destination.mkdir(parents=True, exist_ok=True)
    receipt = {"schema": 1, "machine": platform.machine(),
               "output_base": str(output_base), "builders": [], "rule_sources": [],
               "limits": {"builders": MAX_BUILDERS, "directories": MAX_DIRECTORIES},
               "truncated": False, "directories_visited": 0}
    seen = set()
    roots = [output_base / "execroot/_main/bazel-out", output_base / "sandbox"]
    for root in roots:
        for directory, children, _ in os.walk(root, followlinks=False):
            receipt["directories_visited"] += 1
            if receipt["directories_visited"] > MAX_DIRECTORIES:
                receipt["truncated"] = True
                break
            candidates = []
            if Path(directory).name == "builder_reset":
                candidates.append(Path(directory) / "builder")
            if "builder_reset" in children:
                candidates.append(Path(directory) / "builder_reset/builder")
            for candidate in candidates:
                if str(candidate) in seen:
                    continue
                if len(seen) >= MAX_BUILDERS:
                    receipt["truncated"] = True
                    break
                seen.add(str(candidate))
                receipt["builders"].append(inspect_builder(candidate))
            if receipt["truncated"]:
                break
        if receipt["truncated"]:
            break
    # Record only the two relevant selected rule files, without copying sources.
    for name in ("stdlib.go", "env.go"):
        source = output_base / "external/rules_go+/go/tools/builders" / name
        entry = {"path": str(source)}
        try:
            entry["size"] = source.stat().st_size
            if entry["size"] <= 1024 * 1024:
                entry["sha256"] = sha256(source)
            else:
                entry["inspection_skipped"] = "Selected rule source exceeds 1 MiB limit"
        except OSError as error:
            entry["error"] = str(error)
        receipt["rule_sources"].append(entry)
    lock = Path("MODULE.bazel.lock")
    if lock.is_file() and lock.stat().st_size <= 16 * 1024 * 1024:
        shutil.copyfile(lock, destination / lock.name)
        receipt["module_lock_sha256"] = sha256(lock)
    else:
        receipt["module_lock_unavailable"] = True
    (destination / "go-builder.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-base", required=True, type=Path)
    parser.add_argument("--destination", required=True, type=Path)
    args = parser.parse_args()
    collect(args.output_base, args.destination)


if __name__ == "__main__":
    main()
