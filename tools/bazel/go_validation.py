#!/usr/bin/env python3
"""Record maintained registry inputs and verify resolved Go build evidence."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]
REGISTRY_REPO = "https://github.com/sonic-net/sonic-bazel-registry.git"
MAIN = "https://raw.githubusercontent.com/sonic-net/sonic-bazel-registry/main"
VERSION = "0.65.0.sonic.2"
CGO_SHA256 = "4eaf0252856619ca376e9f32782db84748689e9b68ddcfcd75dfa5fcd45861ad"


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def selected_registry():
    """Select main unless Draft CI explicitly supplies its reviewed branch."""
    url = os.environ.get("SONIC_BAZEL_REGISTRY_URL", MAIN)
    prefix = MAIN.removesuffix("main")
    branch = url.removeprefix(prefix)
    if (not url.startswith(prefix)
            or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_./-]*", branch) is None
            or any(part in branch for part in ("..", "//", "/."))
            or branch.endswith(("/", ".", ".lock"))
            or re.fullmatch(r"[0-9a-fA-F]{40}", branch)):
        raise ValueError("Expected a maintained or reviewed Draft SONiC registry branch URL")
    return url, "refs/heads/" + branch


def check_registry_options(output):
    registry, _ = selected_registry()
    options = re.findall(r"--registry=(\S+)", output)
    sonic_options = [url for url in options if "sonic-net/sonic-bazel-registry/" in url]
    if sonic_options != [registry]:
        raise ValueError("The effective Bazel options must select exactly the recorded SONiC registry endpoint")


def record(artifacts):
    configuration = (ROOT / ".bazelrc").read_text()
    endpoints = [line for line in configuration.splitlines()
                 if "--registry=" in line and "sonic-net/sonic-bazel-registry/" in line]
    if endpoints != ["common --registry=" + MAIN]:
        raise ValueError("Expected exactly the canonical SONiC main registry endpoint")
    registry, ref = selected_registry()
    revision = subprocess.check_output(
        ["git", "-c", "safe.directory=" + str(ROOT), "ls-remote", REGISTRY_REPO,
         ref], cwd=ROOT, text=True).split()
    if (len(revision) != 2 or revision[1] != ref
            or re.fullmatch(r"[0-9a-f]{40}", revision[0]) is None):
        raise ValueError("Could not determine the selected registry branch revision")
    expected_revision = os.environ.get("SONIC_BAZEL_REGISTRY_REVISION")
    if expected_revision is not None and expected_revision != revision[0]:
        raise ValueError("The registry branch differs from the revision selected for this validation")
    (artifacts / "declared.bazelrc").write_text(configuration)
    effective = artifacts.resolve() / "effective.bazelrc"
    effective.write_text(configuration.replace(MAIN, registry))
    if registry != MAIN and os.environ.get("GITHUB_PATH"):
        # Draft CI replaces the workspace registry instead of appending another
        # --registry option. Keep the tracked main declaration unchanged.
        binary = shutil.which("bazel")
        if not binary:
            raise ValueError("Bazel must be installed before selecting Draft inputs")
        commands = artifacts.resolve() / "draft-bin"
        commands.mkdir(exist_ok=True)
        wrapper = commands / "bazel"
        wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(binary) +
                           " --noworkspace_rc --bazelrc=" + shlex.quote(str(effective)) +
                           ' "$@"\n')
        wrapper.chmod(0o755)
        with open(os.environ["GITHUB_PATH"], "a") as path_file:
            path_file.write(str(commands) + "\n")
    write_json(artifacts / "registry-inputs.json", {
        "declared_registry": MAIN,
        "registry": registry, "observed_revision": revision[0],
        "draft_validation": registry != MAIN,
        "module": "rules_go@" + VERSION,
    })


def collect(artifacts, config, output_base_file=None):
    errors = []
    receipts = {}
    try:
        recorded = json.loads((artifacts / "registry-inputs.json").read_text())
        if recorded["registry"] != selected_registry()[0]:
            raise ValueError("Registry selection changed after recording validation inputs")
        bazel = ["bazel"]
        if output_base_file:
            bazel.append("--output_base=" + output_base_file.read_text().strip())
        commands = {
            "bazel-version.txt": [*bazel, "version"],
            "module-graph.txt": [*bazel, "mod", "graph", "--lockfile_mode=update"],
            "rules-go-repository.txt": [*bazel, "mod", "show_repo", "@io_bazel_rules_go"],
            "go-version.txt": [*bazel, "run", *config, "@io_bazel_rules_go//go", "--", "version"],
            "output-base.txt": [*bazel, "info", "--announce_rc", "output_base"],
        }
        for name, command in commands.items():
            with (artifacts / name).open("w") as output:
                result = subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
            receipts[name] = result.returncode
            if result.returncode:
                errors.append(name + " failed")
        output_lines = (artifacts / "output-base.txt").read_text().splitlines()
        try:
            check_registry_options("\n".join(output_lines))
        except ValueError as error:
            errors.append(str(error))
        bases = [Path(line) for line in output_lines if line.startswith("/") and Path(line).is_dir()]
        if not bases:
            raise ValueError("Bazel output base was not recorded")
        repository = bases[-1] / "external/rules_go+"
        module = (repository / "MODULE.bazel").read_text()
        cgo = (repository / "go/private/rules/cgo.bzl").read_bytes()
        actual_hash = hashlib.sha256(cgo).hexdigest()
        (artifacts / "rules-go-MODULE.bazel").write_text(module)
        (artifacts / "rules-go-cgo.bzl").write_bytes(cgo)
        receipts["cgo_sha256"] = actual_hash
        if 'version = "' + VERSION + '"' not in module or actual_hash != CGO_SHA256:
            errors.append("The resolved rules_go version or cgo repair differs from the reviewed entry")
        if "Build label: 8.5.1" not in (artifacts / "bazel-version.txt").read_text().splitlines():
            errors.append("Bazel is not 8.5.1")
        goarch = "arm64" if "--config=aarch64" in config else "amd64"
        if "go version go1.25.0 linux/" + goarch not in (artifacts / "go-version.txt").read_text().splitlines():
            errors.append("The Go SDK is not the expected native 1.25.0")
        if output_base_file:
            with (artifacts / "bazel-shutdown.txt").open("w") as output:
                subprocess.run([*bazel, "shutdown"], cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
    except Exception as error:
        errors.append(str(error))
        raise
    finally:
        shutil.copyfile(ROOT / ".bazelrc", artifacts / "final.bazelrc")
        if (ROOT / "MODULE.bazel.lock").is_file():
            shutil.copyfile(ROOT / "MODULE.bazel.lock", artifacts / "MODULE.bazel.lock")
        write_json(artifacts / "go-input-checks.json", {"receipts": receipts, "errors": errors})
        with (artifacts / "tracked-files.diff").open("w") as output:
            subprocess.run(["git", "-c", "safe.directory=" + str(ROOT), "diff", "--exit-code"],
                           cwd=ROOT, stdout=output, check=True)
    if errors:
        raise ValueError("; ".join(errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["record", "collect"])
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("--config", choices=["aarch64", "ci-linux-sandbox"], action="append", default=[])
    parser.add_argument("--output-base-file", type=Path)
    args = parser.parse_args()
    artifacts = args.artifacts.resolve()
    artifacts.mkdir(parents=True, exist_ok=True)
    if args.action == "record":
        record(artifacts)
    else:
        collect(artifacts, ["--config=" + config for config in args.config], args.output_base_file)


if __name__ == "__main__":
    main()
