#!/usr/bin/env python3
"""Install missing runtime dependencies in a disposable Trixie test container."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from urllib.parse import urlsplit


RUNTIME_ROOTS = (
    "libhiredis1.1.0", "libzmq5", "libboost-serialization1.83.0", "libuuid1",
)


def require_container():
    if os.geteuid() != 0 or not any(Path(p).exists() for p in ("/.dockerenv", "/run/.containerenv")):
        raise RuntimeError("Run this setup as root inside a disposable test container")
    release = Path("/etc/os-release").read_text()
    if "VERSION_CODENAME=trixie" not in release:
        raise RuntimeError("The test container must use Debian Trixie")


def require_no_installed_common():
    for directory in ("/lib", "/usr/lib", "/usr/local/lib"):
        for pattern in ("libswsscommon.so*", "libsonicdbcli.so*"):
            root = Path(directory)
            if list(root.glob(pattern)) or list(root.glob("*/" + pattern)):
                raise RuntimeError("System Common/CLI libraries would mask the current Bazel outputs")


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def runtime_catalog(repositories, architecture):
    hub = repositories["trixie"]["attributes"]
    if "lock_content" in hub:
        packages = json.loads(hub["lock_content"])["packages"]
        return packages, packages.keys()

    # The shared lock is exported by a sibling repository in the APT extension.
    # Only roots selected for this dependency set may seed the runtime closure.
    repository, separator, filename = hub["lock_file"].partition("//:")
    if not separator or not repository.startswith("@@") or filename != "apt.lock.json":
        raise ValueError("Invalid exported APT lock label")
    export = repositories.get(repository.rsplit("+", 1)[-1])
    if not export or not export["repoRuleId"].endswith("%export_lock_repository"):
        raise ValueError("Missing exported APT lock in the resolved extension")
    document = json.loads(export["attributes"]["content"])
    roots = document["dependency_sets"][hub["depset_name"]]["sets"][architecture]
    return document["packages"], [f"{key}={version}" for key, version in roots.items()]


def select_runtime(lock, architecture):
    extensions = [value["general"] for key, value in lock["moduleExtensions"].items()
                  if key.endswith("//apt:extensions.bzl%apt")]
    if len(extensions) != 1:
        raise ValueError("Expected one resolved Distroless apt extension")
    repositories = extensions[0]["generatedRepoSpecs"]
    packages, root_ids = runtime_catalog(repositories, architecture)
    selected = {}
    pending = []
    for root in RUNTIME_ROOTS:
        matches = [key for key in root_ids
                   if packages[key]["name"] == root and packages[key]["architecture"] == architecture]
        if len(matches) != 1:
            raise ValueError(f"Expected one selected {root} package for {architecture}")
        pending.extend(matches)
    while pending:
        identity = pending.pop()
        package = packages[identity]
        name = package["name"]
        if name in selected:
            if selected[name]["version"] != package["version"]:
                raise ValueError(f"Conflicting selected versions of {name}")
            continue
        if name.endswith("-dev") or package["architecture"] not in (architecture, "all"):
            raise ValueError(f"Unexpected package in runtime closure: {identity}")
        matches = [spec["attributes"] for spec in repositories.values()
                   if spec["repoRuleId"].endswith("%deb_import")
                   and spec["attributes"].get("package_name") == name
                   and spec["attributes"].get("sha256") == package["sha256"]]
        if len(matches) != 1:
            raise ValueError(f"Missing or ambiguous download provenance for {identity}")
        selected[name] = {key: package[key] for key in ("name", "version", "architecture", "suite", "sha256")}
        selected[name]["urls"] = matches[0]["urls"]
        pending.extend(package["depends_on"])
    return dict(sorted(selected.items()))


def select_locked_runtime(lock, architecture):
    from sonic_apt.inputs import subset_inputs

    # Select the same versioned lock used by the reproducible APT extension.
    # Its generated repositories are no longer serialized in MODULE.bazel.lock.
    _, manifest = subset_inputs(lock, "trixie", architecture, list(RUNTIME_ROOTS))
    selected = {}
    for identity, row in manifest["packages"].items():
        name = row["name"]
        if name.endswith("-dev") or row["architecture"] not in (architecture, "all"):
            raise ValueError(f"Unexpected package in runtime closure: {identity}")
        selected[name] = {key: row[key] for key in ("name", "version", "architecture", "sha256", "urls")}
        selected[name]["suite"] = lock["packages"][identity]["suite"]
    return dict(sorted(selected.items()))


def snapshot_sources(selected, architecture):
    sources = set()
    for package in selected.values():
        url = urlsplit(package["urls"][0])
        match = re.fullmatch(r"(/archive/(?:debian|debian-security)/[0-9]{8}T[0-9]{6}Z)/pool/.+", url.path)
        if url.scheme != "https" or url.netloc != "snapshot.debian.org" or not match:
            raise ValueError("Runtime inputs must come from the selected Debian snapshots")
        sources.add(f"deb [arch={architecture} check-valid-until=no] https://{url.netloc}{match[1]} {package['suite']} main")
    return "\n".join(sorted(sources)) + "\n"


def inventory():
    output = subprocess.check_output([
        "dpkg-query", "-W", "-f=${Package}\t${Version}\t${Architecture}\t${db:Status-Status}\n",
    ], text=True)
    return {f"{name}:{arch}": version for name, version, arch, status in
            (line.split("\t") for line in output.splitlines()) if status == "installed"}


def missing_packages(selected, before):
    return [f"{name}:{package['architecture']}={package['version']}"
            for name, package in selected.items()
            if f"{name}:{package['architecture']}" not in before]


def verify_inventory(selected, before, after):
    for package, version in before.items():
        if after.get(package) != version:
            raise ValueError(f"APT changed a prepared-base package: {package}")
    expected_new = {f"{name}:{p['architecture']}": p["version"] for name, p in selected.items()
                    if f"{name}:{p['architecture']}" not in before}
    actual_new = {key: value for key, value in after.items() if key not in before}
    if actual_new != expected_new:
        raise ValueError("Installed runtime versions differ from the selected snapshot packages")


def install_missing(selected, architecture, before, log):
    requested = missing_packages(selected, before)
    if not requested:
        return
    with tempfile.TemporaryDirectory(prefix="sonic-test-runtime-") as temporary:
        root = Path(temporary)
        root.chmod(0o755)  # Allow APT's unprivileged downloader to read its inputs.
        (root / "sources.list").write_text(snapshot_sources(selected, architecture))
        (root / "preferences").touch()
        for directory in ("lists/partial", "archives/partial", "sources.list.d", "preferences.d"):
            (root / directory).mkdir(parents=True)
        apt = ["apt-get", "-o", f"Dir::Etc::sourcelist={root / 'sources.list'}",
               "-o", f"Dir::Etc::sourceparts={root / 'sources.list.d'}",
               "-o", f"Dir::Etc::preferences={root / 'preferences'}",
               "-o", f"Dir::Etc::preferencesparts={root / 'preferences.d'}",
               "-o", f"Dir::State::lists={root / 'lists'}",
               "-o", f"Dir::Cache::archives={root / 'archives'}",
               "-o", "Acquire::Check-Valid-Until=false"]
        environment = dict(os.environ, DEBIAN_FRONTEND="noninteractive")
        def run(arguments):
            subprocess.run([*apt, *arguments], env=environment, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        run(["update"])
        arguments = ["--yes", "--no-remove", "--no-install-recommends", "install", *requested]
        # APT resolves and downloads packages; the helper only verifies its plan
        # and hashes. It does not unpack or install Debian packages itself.
        plan = subprocess.check_output([*apt, "--simulate", *arguments], env=environment, text=True)
        log.write(plan)
        for line in plan.splitlines():
            if line.startswith("Remv ") or re.match(r"Inst \S+ \[", line):
                raise ValueError("Runtime setup must not remove or change prepared-base packages")
        run(["--download-only", *arguments])
        for archive in (root / "archives").glob("*.deb"):
            fields = subprocess.check_output([
                "dpkg-deb", "-f", archive, "Package", "Version", "Architecture",
            ], text=True)
            metadata = dict(line.split(": ", 1) for line in fields.splitlines())
            package = selected.get(metadata["Package"])
            if (not package or metadata["Version"] != package["version"]
                    or metadata["Architecture"] != package["architecture"]
                    or sha256(archive) != package["sha256"]):
                raise ValueError(f"APT downloaded an unselected runtime package: {archive.name}")
        run(["--no-download", *arguments])
    subprocess.run(["ldconfig"], check=True)


def library_hashes(selected):
    libraries = {}
    for name, package in selected.items():
        files = subprocess.check_output([
            "dpkg-query", "-L", f"{name}:{package['architecture']}",
        ], text=True).splitlines()
        for filename in files:
            path = Path(filename)
            if ".so" not in path.name or not path.is_file():
                continue
            with path.open("rb") as stream:
                if stream.read(4) != b"\x7fELF":
                    continue
            libraries[str(path)] = {"resolved_path": str(path.resolve()), "sha256": sha256(path)}
    if not libraries:
        raise ValueError("No installed runtime shared libraries were recorded")
    return dict(sorted(libraries.items()))


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument("--apt-lock", type=Path)
    inputs.add_argument("--lock", type=Path, default=Path("MODULE.bazel.lock"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--image-reference", required=True)
    parser.add_argument("--env-file", type=Path)
    args = parser.parse_args()
    require_container()
    require_no_installed_common()
    architecture = subprocess.check_output(["dpkg", "--print-architecture"], text=True).strip()
    if architecture not in ("amd64", "arm64"):
        raise ValueError("Only native AMD64 and ARM64 test containers are supported")
    lock = args.apt_lock or args.lock
    document = json.loads(lock.read_text())
    selected = (select_locked_runtime(document, architecture) if args.apt_lock
                else select_runtime(document, architecture))
    # Bazel runs from the runfiles tree. Resolve operator outputs in the workspace.
    workspace = Path(os.environ.get("BUILD_WORKSPACE_DIRECTORY", "."))
    args.output = workspace / args.output
    if args.env_file:
        args.env_file = workspace / args.env_file
    # Validate provenance even when an earlier setup already installed all roots.
    snapshot_sources(selected, architecture)
    before = inventory()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.with_suffix(".apt.log").open("w") as log:
        install_missing(selected, architecture, before, log)
    after = inventory()
    verify_inventory(selected, before, after)
    require_no_installed_common()
    receipt = {
        "generated": "Generated by tools/bazel/runtime/prepare.py; do not edit manually",
        "validation": "prepared-container runtime",
        "architecture": architecture,
        "image_reference": args.image_reference,
        "os_release": Path("/etc/os-release").read_text(),
        "lock_sha256": sha256(lock),
        "build_packages": selected,
        "installed_packages": after,
        "preserved_base_packages": before,
        "runtime_libraries": library_hashes(selected),
    }
    # Installation history and unrelated lock extensions are evidence, not
    # runtime inputs. Repeating setup for the same runtime must keep its identity.
    receipt["SONIC_TEST_RUNTIME_ID"] = fingerprint({
        key: value for key, value in receipt.items()
        if key not in ("preserved_base_packages", "lock_sha256")
    })
    args.output.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    if args.env_file:
        with args.env_file.open("a") as output:
            output.write("SONIC_TEST_RUNTIME_ID=" + receipt["SONIC_TEST_RUNTIME_ID"] + "\n")
    print("Prepared test runtime " + receipt["SONIC_TEST_RUNTIME_ID"])


if __name__ == "__main__":
    main()
