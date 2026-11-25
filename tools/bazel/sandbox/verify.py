#!/usr/bin/env python3
"""Check processwrapper rejection and Linux sandbox success; retain safe evidence."""

import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]
TARGET = "//tools/bazel/sandbox:isolation_test"
PREFIX = "SONIC_SANDBOX_PROBE="


def execution_records(path):
    decoder = json.JSONDecoder()
    remaining = path.read_text().strip()
    while remaining:
        record, end = decoder.raw_decode(remaining)
        yield record
        remaining = remaining[end:].lstrip()


def run_probe(strategy, extra, context, temporary):
    log = temporary / (strategy + ".json")
    command = ["bazel", "test", *extra, "--spawn_strategy=" + strategy,
               "--strategy=Genrule=" + strategy, "--strategy=TestRunner=" + strategy,
               "--noincompatible_legacy_local_fallback", "--nocache_test_results",
               "--runs_per_test=1", "--flaky_test_attempts=1", "--test_output=all",
               "--color=no", "--curses=no", "--execution_log_json_file=" + str(log),
               "--test_env=SONIC_SANDBOX_CONTEXT=" + json.dumps(context), TARGET]
    result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    # Raw logs may contain environment credentials; never copy them to artifacts.
    (temporary / (strategy + ".log")).write_text(result.stdout)
    proofs = [json.loads(line[len(PREFIX):]) for line in result.stdout.splitlines() if line.startswith(PREFIX)]
    if len(proofs) != 1 or not log.exists():
        # Ordinary Bazel diagnostics belong in the job log, not raw action logs.
        print(result.stdout, flush=True)
        raise ValueError(strategy + ": missing unique test proof or execution log")
    # Bazel also labels its XML-report helper TestRunner. Only the test process
    # declares the premature-exit marker; the report helper declares test.xml.
    runners = [record for record in execution_records(log)
               if record.get("mnemonic") == "TestRunner" and record.get("targetLabel") == TARGET
               and any(path.endswith("/test.exited_prematurely")
                       for path in record.get("listedOutputs", []))]
    if len(runners) != 1 or runners[0].get("runner") != strategy or runners[0].get("cacheHit", False):
        raise ValueError(strategy + ": TestRunner did not execute with the required strategy")
    proof = proofs[0]
    expected_failure = strategy == "processwrapper-sandbox"
    expected_errors = [name + "_namespace_not_isolated" for name in ("user", "mnt", "pid")] + ["outside_write_allowed"]
    if (result.returncode != (3 if expected_failure else 0)
            or runners[0].get("exitCode", 0) != (1 if expected_failure else 0)
            or proof["failures"] != (expected_errors if expected_failure else [])
            or proof["marker_write_denied"] == expected_failure):
        raise ValueError(strategy + ": unexpected probe result; infrastructure failures are not valid negative controls")
    return {"runner": strategy, "bazel_exit_code": result.returncode, "probe": proof}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("bazel_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    extra = args.bazel_args[1:] if args.bazel_args[:1] == ["--"] else args.bazel_args
    receipt = {"architecture": platform.machine(), "target": TARGET, "passed": False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    try:
        # /tmp is private in Linux sandboxes; /opt gives a visible read-only marker.
        with tempfile.TemporaryDirectory(prefix="sonic-sandbox-", dir="/opt") as directory:
            temporary = Path(directory)
            temporary.chmod(0o755)
            marker = temporary / "host-marker"
            marker.write_text("sandbox probe: this file must not be modified\n")
            marker.chmod(0o666)
            descriptor = os.open(marker, os.O_WRONLY)
            os.close(descriptor)
            context = {"marker": str(marker), "namespaces": {
                name: os.readlink("/proc/self/ns/" + name) for name in ("user", "mnt", "pid")}}
            receipt["parent_namespaces"] = context["namespaces"]
            receipt["parent_marker_writable"] = True
            receipt["negative_control"] = run_probe("processwrapper-sandbox", extra, context, temporary)
            receipt["positive_control"] = run_probe("linux-sandbox", extra, context, temporary)
            receipt["passed"] = True
    except Exception as failure:
        receipt["error"] = str(failure)
        raise
    finally:
        args.output.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print("Verified Linux sandbox isolation: " + str(args.output))


if __name__ == "__main__":
    main()
