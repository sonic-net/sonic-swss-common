"""Check cache seed policy and anonymous validation readback."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest import TestCase, main, mock

import validate


class UploadPolicyTest(TestCase):
    def test_readback_selects_only_the_anonymous_config(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(validate, "command", side_effect=RuntimeError("captured")) as command:
            with self.assertRaisesRegex(RuntimeError, "captured"):
                validate.build_phase("read", Path(directory), [])
            options = command.call_args.args[0]
            self.assertIn("--nosystem_rc", options)
            self.assertIn("--nohome_rc", options)
            self.assertIn("--config=ci-remote-cache", options)
            self.assertIn("--remote_upload_local_results=false", options)
            self.assertNotIn("--config=ci-remote-cache-write", options)

    def test_seed_rejects_pr_feature_tag_and_local_context_before_bazel(self):
        for context in (
                {"GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "pull_request", "GITHUB_REF": "refs/pull/35/merge"},
                {"GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "pull_request_target", "GITHUB_REF": "refs/heads/master"},
                {"GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "workflow_dispatch", "GITHUB_REF": "refs/heads/feature"},
                {"GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/tags/master"},
                {}):
            with self.subTest(context=context), mock.patch.dict(os.environ, context, clear=True), \
                    mock.patch.object(validate, "command") as command, \
                    tempfile.TemporaryDirectory() as directory:
                with self.assertRaisesRegex(ValueError, "master branch CI"):
                    validate.build_phase("seed", Path(directory), [])
                command.assert_not_called()


class CacheReceiptTest(TestCase):
    def test_seed_and_read_must_produce_the_same_library(self):
        for same_library in (True, False):
            with self.subTest(same_library=same_library), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp)
                receipt = directory / "receipt.json"

                def bazel(command, **kwargs):
                    # The real commands run in separate output bases. Feed the
                    # validator their execution logs and materialized libraries.
                    options = dict(arg.split("=", 1) for arg in command if "=" in arg)
                    base = Path(options["--output_base"])
                    seed = base.name == "seed-output"
                    library = base / "libswsscommon.so.0"
                    if "build" in command:
                        base.mkdir()
                        library.write_bytes(b"compiled" if seed or same_library else b"different")
                        actions = [{"mnemonic": mnemonic, "targetLabel": "@@" + target,
                                    "runner": "linux-sandbox" if seed else "remote cache hit",
                                    "cacheHit": not seed}
                                   for mnemonic, target in validate.COMMON_ACTIONS.items()]
                        Path(options["--execution_log_json_file"]).write_text(
                            "\n".join(json.dumps(action) for action in actions))
                        output = ""
                    else:
                        self.assertIn("cquery", command)
                        output = str(library) + "\n"
                    return subprocess.CompletedProcess(command, 0, stdout=output, stderr="")

                env = {"GITHUB_ACTIONS": "true", "GITHUB_EVENT_NAME": "push",
                       "GITHUB_REF": "refs/heads/master", "BAZEL_REMOTE_AUTH": "test-token",
                       "RUNNER_TEMP": temp}
                with mock.patch.dict(os.environ, env, clear=True), \
                        mock.patch.object(sys, "argv", ["validate.py", "--output", str(receipt)]), \
                        mock.patch.object(validate.subprocess, "run", side_effect=bazel):
                    if same_library:
                        validate.main()
                    else:
                        with self.assertRaisesRegex(ValueError, "libraries differ"):
                            validate.main()

                result = json.loads(receipt.read_text())
                self.assertEqual(result["passed"], same_library)
                seed, read = result["phases"]
                self.assertEqual([seed["phase"], read["phase"]], ["seed", "read"])
                self.assertEqual(seed["library_sha256"], hashlib.sha256(b"compiled").hexdigest())
                self.assertEqual({row["runner"] for row in read["common_actions"]}, {"remote cache hit"})

    def test_compiling_during_readback_invalidates_remote_hit_evidence(self):
        actions = [{"mnemonic": "CppCompile", "runner": "linux-sandbox", "cache_hit": False},
                   {"mnemonic": "CppLink", "runner": "remote cache hit", "cache_hit": True}]
        with self.assertRaisesRegex(ValueError, "Missing Common CppCompile remote cache hit"):
            validate.check_common_actions({"common_actions": actions}, remote=True)


if __name__ == "__main__":
    main()
