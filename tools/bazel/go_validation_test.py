"""Keep normal registry validation strict while permitting explicit Draft CI."""

import json
import os
from pathlib import Path
import tempfile
import subprocess
import unittest
from unittest.mock import patch

import go_validation as validation


class RegistrySelectionTest(unittest.TestCase):
    DRAFT = validation.MAIN.removesuffix("main") + "codex/shared-serde-validation"
    REVISION = "a" * 40

    def test_default_requires_only_main(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(validation.selected_registry(), (validation.MAIN, "refs/heads/main"))
            validation.check_registry_options("--registry=" + validation.MAIN + " --registry=https://bcr.bazel.build")
            with self.assertRaises(ValueError):
                validation.check_registry_options("--registry=" + self.DRAFT)

    def test_draft_replaces_main_instead_of_adding_a_fallback(self):
        with patch.dict(os.environ, {"SONIC_BAZEL_REGISTRY_URL": self.DRAFT}, clear=True):
            validation.check_registry_options("--registry=" + self.DRAFT)
            for options in (
                "--registry=" + validation.MAIN,
                "--registry=" + self.DRAFT + " --registry=" + validation.MAIN,
                "--registry=" + self.DRAFT + " --registry=" + self.DRAFT,
            ):
                with self.subTest(options=options), self.assertRaises(ValueError):
                    validation.check_registry_options(options)

    def test_arbitrary_urls_and_commit_pins_are_rejected(self):
        for url in (
            "https://example.com/registry/main",
            validation.MAIN.removesuffix("main") + self.REVISION,
            validation.MAIN + "/../other",
            self.DRAFT + "?query=1",
            self.DRAFT + "/",
        ):
            with self.subTest(url=url), patch.dict(os.environ, {"SONIC_BAZEL_REGISTRY_URL": url}, clear=True):
                with self.assertRaises(ValueError):
                    validation.selected_registry()

    def test_record_keeps_main_declaration_and_observes_selected_branch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            configuration = "common --registry=" + validation.MAIN + "\n"
            (root / ".bazelrc").write_text(configuration)
            artifacts = root / "artifacts"
            artifacts.mkdir()
            effective = configuration.replace(validation.MAIN, self.DRAFT)
            (artifacts / "effective.bazelrc").write_text(effective)
            environment = {
                "SONIC_BAZEL_REGISTRY_URL": self.DRAFT,
                "SONIC_BAZEL_REGISTRY_REVISION": self.REVISION,
            }
            ref = "refs/heads/codex/shared-serde-validation"
            with patch.object(validation, "ROOT", root), patch.dict(os.environ, environment, clear=True), \
                    patch.object(validation.subprocess, "check_output", return_value=self.REVISION + "\t" + ref + "\n") as remote:
                validation.record(artifacts)
                self.assertEqual(remote.call_args.args[0][-1], ref)
                receipt = json.loads((artifacts / "registry-inputs.json").read_text())
                self.assertEqual(receipt["registry"], self.DRAFT)
                self.assertEqual(receipt["observed_revision"], self.REVISION)
                self.assertTrue(receipt["draft_validation"])
                self.assertEqual((artifacts / "declared.bazelrc").read_text(), configuration)
                self.assertEqual((artifacts / "effective.bazelrc").read_text(), effective)
                with patch.dict(os.environ, {"SONIC_BAZEL_REGISTRY_REVISION": "b" * 40}):
                    with self.assertRaisesRegex(ValueError, "differs"):
                        validation.record(artifacts)
                (root / ".bazelrc").write_text(configuration.replace(validation.MAIN, self.DRAFT))
                with self.assertRaisesRegex(ValueError, "canonical"):
                    validation.record(artifacts)

    def test_draft_ci_wrapper_replaces_workspace_rc_and_preserves_arguments(self):
        """Every CI Bazel subprocess sees one registry and unchanged caller flags."""
        with tempfile.TemporaryDirectory(prefix="draft registry ") as directory:
            root = Path(directory)
            (root / ".bazelrc").write_text("common --registry=" + validation.MAIN + "\n")
            artifacts = root / "artifacts"
            artifacts.mkdir()
            path_file = root / "github-path"
            binary = root / "real-bazel"
            binary.write_text("#!/bin/sh\nprintf '%s\\n' \"$@\"\n")
            binary.chmod(0o755)
            environment = {"SONIC_BAZEL_REGISTRY_URL": self.DRAFT,
                           "GITHUB_PATH": str(path_file)}
            ref = "refs/heads/codex/shared-serde-validation"
            with patch.object(validation, "ROOT", root), patch.dict(os.environ, environment, clear=True), \
                    patch.object(validation.shutil, "which", return_value=str(binary)), \
                    patch.object(validation.subprocess, "check_output", return_value=self.REVISION + "\t" + ref + "\n"):
                validation.record(artifacts)
            wrapper = Path(path_file.read_text().strip()) / "bazel"
            result = subprocess.check_output([str(wrapper), "--output_base=/tmp/build space", "test", "//:target"], text=True)
            self.assertEqual(result.splitlines(), ["--noworkspace_rc",
                             "--bazelrc=" + str(artifacts / "effective.bazelrc"),
                             "--output_base=/tmp/build space", "test", "//:target"])
            self.assertNotIn(validation.MAIN, (artifacts / "effective.bazelrc").read_text())
            self.assertIn(validation.MAIN, (root / ".bazelrc").read_text())

    def test_main_record_retains_effective_configuration_without_draft_helper(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            configuration = "common --registry=" + validation.MAIN + "\n"
            (root / ".bazelrc").write_text(configuration)
            artifacts = root / "artifacts"
            artifacts.mkdir()
            with patch.object(validation, "ROOT", root), patch.dict(os.environ, {}, clear=True), \
                    patch.object(validation.subprocess, "check_output", return_value=self.REVISION + "\trefs/heads/main\n"):
                validation.record(artifacts)
            self.assertEqual((artifacts / "declared.bazelrc").read_text(), configuration)
            self.assertEqual((artifacts / "effective.bazelrc").read_text(), configuration)
            receipt = json.loads((artifacts / "registry-inputs.json").read_text())
            self.assertFalse(receipt["draft_validation"])


if __name__ == "__main__":
    unittest.main()
