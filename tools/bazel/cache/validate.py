#!/usr/bin/env python3
"""Prove Common compilation and linking reuse the remote cache on a fresh build."""

import argparse
import base64
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import time

import policy


ROOT = Path(__file__).resolve().parents[3]
TARGETS = ["//:libswsscommon_shared", "//:libswsscommon_consolidated.so"]
COMMON_ACTIONS = {
    "CppCompile": "//:common",
    "CppLink": "//:libswsscommon_consolidated.so",
}


def records(path):
    """Bazel's execution log is a stream of JSON objects, not a JSON array."""
    text = path.read_text()
    decoder = json.JSONDecoder()
    position = 0
    while position < len(text):
        if text[position].isspace():
            position += 1
            continue
        record, position = decoder.raw_decode(text, position)
        yield record


def summarize(path):
    counts = Counter()
    common = Counter()
    for record in records(path):
        mnemonic = record.get("mnemonic", "")
        runner = record.get("runner", "")
        hit = record.get("cacheHit", False)
        counts[mnemonic, runner, hit] += 1
        target = record.get("targetLabel", "")
        # Canonical labels for the root module may include an empty repo prefix.
        if target.startswith("@@//"):
            target = target[2:]
        if mnemonic in COMMON_ACTIONS and target == COMMON_ACTIONS[mnemonic]:
            if record.get("exitCode", 0) != 0:
                raise ValueError("A Common compile or link action failed")
            common[mnemonic, runner, hit] += 1
    rows = lambda values: [
        {"mnemonic": mnemonic, "runner": runner, "cache_hit": hit, "count": count}
        for (mnemonic, runner, hit), count in sorted(values.items())
    ]
    return {"actions": rows(counts), "common_actions": rows(common)}


def check_common_actions(summary, remote):
    for mnemonic in COMMON_ACTIONS:
        matches = [row for row in summary["common_actions"]
                   if row["mnemonic"] == mnemonic
                   and (row["runner"] == "remote cache hit" and row["cache_hit"]
                        if remote else not row["cache_hit"])]
        if not matches:
            raise ValueError("Missing Common " + mnemonic +
                             (" remote cache hit" if remote else " local execution"))


def print_safe_diagnostics(text):
    """Print ordinary Bazel errors, never raw execution records or credentials."""
    authorization = os.environ.get("BAZEL_REMOTE_AUTH", "")
    secrets = [authorization]
    if authorization.startswith("Basic "):
        encoded = authorization.removeprefix("Basic ")
        secrets.append(encoded)
        try:
            password = base64.b64decode(encoded, validate=True).decode().split(":", 1)[1]
        except (ValueError, UnicodeError, IndexError):
            # Malformed authentication has no decodable password. The original
            # header and encoded token remain in secrets and are still redacted.
            password = ""
        secrets.append(password)
    for secret in secrets:
        if secret:
            text = text.replace(secret, "[REDACTED]")
    print("\n".join(text.splitlines()[-80:]), flush=True)


def command(args, log):
    result = subprocess.run(args, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        print_safe_diagnostics(result.stdout + result.stderr)
        operation = next(value for value in args[1:] if not value.startswith("--"))
        raise ValueError("Bazel " + operation + " failed")
    return result.stdout


def build_phase(name, temporary, config):
    seed = name == "seed"
    if seed:
        policy.require_upload_allowed()
    output_base = temporary / (name + "-output")
    if output_base.exists():
        raise ValueError("The validation output base must be new")
    # The disposable job container does not necessarily reap orphaned servers.
    # Batch mode keeps each Bazel JVM attached to its invoking process, so an
    # unrelated shutdown failure cannot mask the cache-validation result.
    # A master user's RC can select the writer helper; readback must stay anonymous.
    prefix = ["bazel", "--batch", "--nosystem_rc", "--nohome_rc",
              "--output_base=" + str(output_base)]
    flags = ["--config=ci-remote-cache", "--config=ci-linux-sandbox", *config,
             *(["--config=ci-remote-cache-write"] if seed else []),
             "--lockfile_mode=update", "--//tools/bazel:yang_modules=False",
             "--disk_cache=", "--remote_executor=", "--remote_download_outputs=all",
             "--remote_cache_async=false", "--color=no", "--curses=no",
             "--remote_upload_local_results=" + str(seed).lower(),
             "--remote_accept_cached=" + str(not seed).lower()]
    actions = temporary / (name + "-actions.json")
    start = time.monotonic()
    print("Remote cache validation: " + name + " Common build", flush=True)
    command([*prefix, "build", *flags, "--execution_log_json_file=" + str(actions),
             *TARGETS], temporary / (name + "-build.log"))
    summary = summarize(actions)
    check_common_actions(summary, remote=not seed)
    files = command([*prefix, "cquery", *flags, "--output=files", TARGETS[1]],
                    temporary / (name + "-query.log")).splitlines()
    libraries = [Path(value) for value in files if Path(value).name == "libswsscommon.so.0"]
    if len(libraries) != 1:
        raise ValueError("Expected exactly one libswsscommon.so.0 output")
    # cquery reports workspace-relative paths through Bazel's convenience
    # symlinks. Resolve the actual built file and reject a stale link to another
    # output base. This avoids an `info` invocation, which does not resolve the
    # apparent Bzlmod platform labels in this Bazel version.
    library = (ROOT / libraries[0]).resolve()
    if not library.is_file() or not library.is_relative_to(output_base.resolve()):
        raise ValueError("The Common shared library was not materialized in this output base")
    summary.update({"phase": name, "fresh_output_base": True, "batch": True,
                    "disk_cache": False, "remote_reads": not seed, "remote_writes": seed,
                    "library": "libswsscommon.so.0",
                    "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
                    "library_bytes": library.stat().st_size,
                    "elapsed_seconds": round(time.monotonic() - start, 3)})
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--config", choices=["aarch64"], action="append", default=[])
    args = parser.parse_args()
    receipt = {"architecture": platform.machine(), "targets": TARGETS,
               "yang": False, "passed": False, "phases": []}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    try:
        policy.require_upload_allowed()
        if not os.environ.get("BAZEL_REMOTE_AUTH"):
            raise ValueError("Remote cache validation requires CI credentials")
        # Raw action records can contain environment values. Keep them private and
        # outside the uploaded artifact directory, retaining only these selected fields.
        with tempfile.TemporaryDirectory(prefix="common-remote-cache-",
                                         dir=os.environ.get("RUNNER_TEMP")) as directory:
            temporary = Path(directory)
            for phase in ("seed", "read"):
                receipt["phases"].append(build_phase(
                    phase, temporary, ["--config=" + value for value in args.config]))
            if receipt["phases"][0]["library_sha256"] != receipt["phases"][1]["library_sha256"]:
                raise ValueError("The seeded and cached Common libraries differ")
            receipt["passed"] = True
    except (OSError, ValueError) as error:
        receipt["error"] = str(error)
        raise
    finally:
        args.output.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print("Verified Common remote cache hits and matching library: " + str(args.output))


if __name__ == "__main__":
    main()
