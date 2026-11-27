"""Check package provenance and test-cache identity without installing anything."""

import copy
import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from unittest import TestCase, main, mock

import prepare


def resolved_lock():
    packages = {}
    repositories = {}
    for architecture in ("amd64", "arm64"):
        for name in (*prepare.RUNTIME_ROOTS, "zlib1g"):
            version = "1:2.3-1" if name == "zlib1g" else "2.3-1"
            identity = f"/trixie/{name}:{architecture}={version}"
            checksum = (name + architecture).encode().hex().ljust(64, "0")
            packages[identity] = {
                "name": name, "version": version, "architecture": architecture,
                "suite": "trixie", "sha256": checksum,
                "depends_on": [] if name == "zlib1g" else [f"/trixie/zlib1g:{architecture}=1:2.3-1"],
            }
            repositories[identity] = {
                "repoRuleId": "@@rules_distroless+//apt/private:deb_import.bzl%deb_import",
                "attributes": {"package_name": name, "sha256": checksum, "urls": [
                    f"https://snapshot.debian.org/archive/debian/20260727T143429Z/pool/main/p/{name}_2.3-1_{architecture}.deb",
                ]},
            }
    repositories["trixie"] = {"repoRuleId": "hub", "attributes": {"lock_content": json.dumps({"packages": packages})}}
    return {"moduleExtensions": {"@@rules_distroless+//apt:extensions.bzl%apt": {
        "general": {"generatedRepoSpecs": repositories},
    }}}


class RuntimePreparationTest(TestCase):
    def test_selects_native_packages_and_preserves_debian_epoch(self):
        for architecture in ("amd64", "arm64"):
            selected = prepare.select_runtime(resolved_lock(), architecture)
            self.assertEqual(len(selected), 5)
            self.assertEqual(selected["zlib1g"]["version"], "1:2.3-1")
            self.assertEqual({p["architecture"] for p in selected.values()}, {architecture})

    def test_selects_the_trixie_set_from_the_exported_multi_suite_lock(self):
        lock = resolved_lock()
        repositories = next(iter(lock["moduleExtensions"].values()))["general"]["generatedRepoSpecs"]
        packages = json.loads(repositories["trixie"]["attributes"]["lock_content"])["packages"]
        sets = {arch: {key.rsplit("=", 1)[0]: p["version"] for key, p in packages.items()
                       if p["architecture"] == arch and p["name"] in prepare.RUNTIME_ROOTS}
                for arch in ("amd64", "arm64")}
        packages["/bookworm/libzmq5:amd64=1"] = dict(packages["/trixie/libzmq5:amd64=2.3-1"],
                                                       version="1", suite="bookworm")
        document = {"packages": packages, "dependency_sets": {"trixie": {"sets": sets}}}
        repositories["exported"] = {"repoRuleId": "apt%export_lock_repository",
                                    "attributes": {"content": json.dumps(document)}}
        repositories["trixie"]["attributes"] = {
            "depset_name": "trixie", "lock_file": "@@rules_distroless++apt+exported//:apt.lock.json"}
        for arch in sets:
            with self.subTest(architecture=arch):
                self.assertEqual(prepare.select_runtime(lock, arch),
                                 prepare.select_runtime(resolved_lock(), arch))

    def test_mismatched_download_provenance_is_rejected(self):
        lock = resolved_lock()
        repositories = next(iter(lock["moduleExtensions"].values()))["general"]["generatedRepoSpecs"]
        repositories["/trixie/libzmq5:amd64=2.3-1"]["attributes"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "download provenance"):
            prepare.select_runtime(lock, "amd64")

    def test_latest_debian_source_is_not_substituted(self):
        selected = prepare.select_runtime(resolved_lock(), "amd64")
        selected["libzmq5"]["urls"] = ["https://deb.debian.org/debian/pool/main/z/zmq.deb"]
        with self.assertRaisesRegex(ValueError, "selected Debian snapshots"):
            prepare.snapshot_sources(selected, "amd64")

    def test_prepared_base_is_preserved_but_missing_packages_use_exact_versions(self):
        selected = prepare.select_runtime(resolved_lock(), "amd64")
        before = {"libuuid1:amd64": "9.0-1", "base-files:amd64": "13.0"}
        requested = prepare.missing_packages(selected, before)
        self.assertFalse(any(item.startswith("libuuid1:") for item in requested))
        after = dict(before, **{f"{name}:amd64": p["version"] for name, p in selected.items() if name != "libuuid1"})
        prepare.verify_inventory(selected, before, after)
        after["libzmq5:amd64"] = "9.0-1"
        with self.assertRaisesRegex(ValueError, "Installed runtime versions"):
            prepare.verify_inventory(selected, before, after)
        after["libuuid1:amd64"] = "2.3-1"
        with self.assertRaisesRegex(ValueError, "prepared-base package"):
            prepare.verify_inventory(selected, before, after)

    def test_fingerprint_changes_with_actual_versions_or_library_bytes(self):
        original = {"installed": {"libc6": "2.41-12"}, "libraries": {"libc.so.6": "old-hash"}}
        reordered = {"libraries": original["libraries"], "installed": original["installed"]}
        self.assertEqual(prepare.fingerprint(original), prepare.fingerprint(reordered))
        changed = copy.deepcopy(original)
        changed["libraries"]["libc.so.6"] = "new-hash"
        self.assertNotEqual(prepare.fingerprint(original), prepare.fingerprint(changed))
        changed = copy.deepcopy(original)
        changed["installed"]["libc6"] = "2.41-13"
        self.assertNotEqual(prepare.fingerprint(original), prepare.fingerprint(changed))

    def test_apt_only_installs_a_verified_snapshot_archive(self):
        archive = b"downloaded package"
        selected = prepare.select_runtime(resolved_lock(), "amd64")
        package = selected["libuuid1"]
        package["sha256"] = hashlib.sha256(archive).hexdigest()
        present = {f"{name}:amd64": row["version"] for name, row in selected.items() if name != "libuuid1"}
        for downloaded in (archive, b"unexpected bytes"):
            with self.subTest(checksum_matches=downloaded == archive):
                calls = []

                def run(command, **kwargs):
                    calls.append(command)
                    if "--download-only" in command:
                        cache = next(arg.split("=", 1)[1] for arg in command
                                     if arg.startswith("Dir::Cache::archives="))
                        (Path(cache) / "libuuid1.deb").write_bytes(downloaded)

                def output(command, **kwargs):
                    if command[0] == "dpkg-deb":
                        return "Package: libuuid1\nVersion: 2.3-1\nArchitecture: amd64\n"
                    self.assertIn("--simulate", command)
                    self.assertIn("libuuid1:amd64=2.3-1", command)
                    return "Inst libuuid1 (2.3-1 Debian:trixie [amd64])\n"

                with mock.patch.object(prepare.subprocess, "run", side_effect=run), \
                        mock.patch.object(prepare.subprocess, "check_output", side_effect=output):
                    if downloaded == archive:
                        prepare.install_missing(selected, "amd64", present, io.StringIO())
                        self.assertIn("--no-download", calls[-2])
                        self.assertEqual(calls[-1], ["ldconfig"])
                    else:
                        with self.assertRaisesRegex(ValueError, "unselected runtime package"):
                            prepare.install_missing(selected, "amd64", present, io.StringIO())
                        self.assertFalse(any("--no-download" in command for command in calls))

    def test_prepared_runtime_receipt_tracks_the_installed_libraries(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            lock = root / "MODULE.bazel.lock"
            lock.write_text(json.dumps(resolved_lock()))
            library = root / "libfixture.so.1"
            library.write_bytes(b"\x7fELFfixture")
            receipt_file = root / "runtime.json"
            environment = root / "env"
            selected = prepare.select_runtime(resolved_lock(), "amd64")

            def dpkg_output(command, **kwargs):
                if command == ["dpkg", "--print-architecture"]:
                    return "amd64\n"
                if command[:2] == ["dpkg-query", "-W"]:
                    return "".join(f"{name}\t{row['version']}\tamd64\tinstalled\n"
                                   for name, row in selected.items())
                self.assertEqual(command[:2], ["dpkg-query", "-L"])
                return str(library) + "\n"

            args = ["prepare.py", "--lock", str(lock), "--output", receipt_file.name,
                    "--image-reference", "trixie", "--env-file", environment.name]
            with mock.patch.dict(os.environ, {"BUILD_WORKSPACE_DIRECTORY": temp}), \
                    mock.patch.object(sys, "argv", args), \
                    mock.patch.object(prepare, "require_container"), \
                    mock.patch.object(prepare, "require_no_installed_common"), \
                    mock.patch.object(prepare.subprocess, "check_output", side_effect=dpkg_output), \
                    mock.patch.object(prepare.subprocess, "run") as install:
                prepare.main()
                first = json.loads(receipt_file.read_text())
                self.assertEqual(first["runtime_libraries"][str(library)]["sha256"],
                                 hashlib.sha256(library.read_bytes()).hexdigest())
                self.assertEqual(environment.read_text(), "SONIC_TEST_RUNTIME_ID=" + first["SONIC_TEST_RUNTIME_ID"] + "\n")
                library.write_bytes(b"\x7fELFchanged")
                prepare.main()
                self.assertNotEqual(first["SONIC_TEST_RUNTIME_ID"],
                                    json.loads(receipt_file.read_text())["SONIC_TEST_RUNTIME_ID"])
                install.assert_not_called()


if __name__ == "__main__":
    main()
