"""Check branch policy and the actual cache setup script without network access."""

import base64
import importlib.util
import itertools
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest import TestCase, main, mock


SCRIPT = Path(__file__).with_name("policy.py")
SPEC = importlib.util.spec_from_file_location("cache_policy", SCRIPT)
cache_policy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cache_policy)
MASTER_PUSH = {"GITHUB_ACTIONS": "true", "GITHUB_REF": "refs/heads/master", "GITHUB_EVENT_NAME": "push"}
AUTH = "Basic " + base64.b64encode(b"test-user:private-test-password").decode()


class PolicyTests(TestCase):
    def test_only_master_push_and_dispatch_allow_uploads(self):
        actions = ("", "false", "true")
        refs = ("", "refs/heads/master", "refs/heads/feature", "refs/tags/master", "refs/pull/35/merge")
        events = ("", "push", "workflow_dispatch", "pull_request", "pull_request_target", "schedule")
        permitted = {("true", "refs/heads/master", "push"),
                     ("true", "refs/heads/master", "workflow_dispatch")}
        for context in itertools.product(actions, refs, events):
            with self.subTest(context=context):
                environ = dict(zip(("GITHUB_ACTIONS", "GITHUB_REF", "GITHUB_EVENT_NAME"), context))
                self.assertIs(cache_policy.upload_allowed(environ), context in permitted)

    def test_empty_explicit_environment_does_not_inherit_ci(self):
        with mock.patch.dict(os.environ, MASTER_PUSH, clear=True):
            self.assertTrue(cache_policy.upload_allowed())
            self.assertFalse(cache_policy.upload_allowed({}))

    def test_seed_guard_allows_master_and_sanitizes_denials(self):
        for event in ("push", "workflow_dispatch"):
            cache_policy.require_upload_allowed({**MASTER_PUSH, "GITHUB_EVENT_NAME": event})
        for context in ({}, {**MASTER_PUSH, "GITHUB_EVENT_NAME": "pull_request"},
                        {**MASTER_PUSH, "GITHUB_REF": AUTH}):
            with self.subTest(context_has_ref="GITHUB_REF" in context):
                with self.assertRaises(ValueError) as error:
                    cache_policy.require_upload_allowed(context)
                self.assertEqual(str(error.exception), cache_policy.ERROR)
                self.assertNotIn(AUTH, str(error.exception))

    def test_cli_prints_only_the_policy_boolean(self):
        for context, expected in (({}, "false\n"), (MASTER_PUSH, "true\n"),
                                  ({**MASTER_PUSH, "GITHUB_EVENT_NAME": "pull_request"}, "false\n")):
            with self.subTest(expected=expected):
                result = subprocess.run([sys.executable, "-B", str(SCRIPT)],
                                        env={**context, "BAZEL_REMOTE_AUTH": AUTH},
                                        text=True, capture_output=True, check=False)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, expected)
                self.assertEqual(result.stderr, "")

    def configure(self, context, auth=AUTH, probe_status=0, probe_receipt=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "tools/bazel/cache"
            cache.mkdir(parents=True)
            shutil.copyfile(SCRIPT, cache / "policy.py")
            shutil.copyfile(SCRIPT.with_name("configure.sh"), cache / "configure.sh")
            (cache / "credentials.py").write_text(
                "import argparse, json\nfrom pathlib import Path\n"
                "parser = argparse.ArgumentParser()\n"
                "parser.add_argument('command', choices=['probe'])\n"
                "parser.add_argument('--anonymous', action='store_true')\n"
                "parser.add_argument('--output', required=True)\n"
                "args = parser.parse_args()\n"
                "Path('probe-called').write_text(json.dumps({'anonymous': args.anonymous}))\n"
                f"probe_receipt = {probe_receipt!r}\n"
                "if probe_receipt is not None:\n"
                "    Path(args.output).write_text(json.dumps(probe_receipt))\n"
                f"status = {probe_status!r}\n"
                "if status: raise SystemExit(status)\n"
                "Path(args.output).write_text(json.dumps({'authenticated': not args.anonymous, 'status': 200}))\n"
            )
            test_home = root / "test-home"
            test_home.mkdir()
            rc = test_home / ".bazelrc"
            existing = "build --jobs=2\n"
            rc.write_text(existing)
            environ = {"PATH": os.defpath, "HOME": str(test_home), **context}
            if auth is not None:
                environ["BAZEL_REMOTE_AUTH"] = auth
            result = subprocess.run(["/bin/bash", str(cache / "configure.sh")], cwd=root,
                                    env=environ, text=True, capture_output=True, check=False)
            receipt = root / "artifacts/validation/remote-cache.json"
            probe = root / "probe-called"
            return (result, rc.read_text(), json.loads(probe.read_text()) if probe.exists() else None,
                    json.loads(receipt.read_text()) if receipt.exists() else None)

    def test_configure_uses_policy_after_successful_probe(self):
        cases = [(MASTER_PUSH, "true"),
                 ({**MASTER_PUSH, "GITHUB_EVENT_NAME": "workflow_dispatch"}, "true"),
                 ({**MASTER_PUSH, "GITHUB_EVENT_NAME": "pull_request"}, "false"),
                 ({**MASTER_PUSH, "GITHUB_REF": "refs/heads/feature"}, "false"),
                 ({**MASTER_PUSH, "GITHUB_EVENT_NAME": "workflow_dispatch", "GITHUB_REF": "refs/heads/feature"}, "false"),
                 ({}, "false")]
        for context, upload in cases:
            with self.subTest(context=context):
                result, rc, probed, receipt = self.configure(context)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(probed, {"anonymous": upload != "true"})
                self.assertEqual(receipt, {"authenticated": upload == "true", "status": 200,
                                          "upload_local_results": upload == "true",
                                          "github_event_name": context.get("GITHUB_EVENT_NAME"),
                                          "github_ref": context.get("GITHUB_REF")})
                writer = "build --config=ci-remote-cache-write\n" if upload == "true" else ""
                self.assertEqual(rc, "build --jobs=2\n\nbuild --config=ci-remote-cache\n" + writer +
                                 "build --remote_upload_local_results=" + upload + "\n")
                self.assertIn("upload local results: " + upload, result.stdout)
                for secret in (AUTH, AUTH[6:], "private-test-password"):
                    self.assertNotIn(secret, result.stdout + result.stderr + rc + json.dumps(receipt))

    def test_missing_auth_keeps_pr_and_master_reads_anonymous(self):
        for context in (MASTER_PUSH, {**MASTER_PUSH, "GITHUB_EVENT_NAME": "pull_request",
                                     "GITHUB_REF": "refs/pull/35/merge"}):
            for auth in (None, ""):
                with self.subTest(context=context, auth_present=auth is not None):
                    result, rc, probed, receipt = self.configure(context, auth=auth)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(probed, {"anonymous": True})
                    self.assertEqual(rc, "build --jobs=2\n\nbuild --config=ci-remote-cache\n"
                                     "build --remote_upload_local_results=false\n")
                    self.assertEqual(receipt, {"authenticated": False, "status": 200,
                                              "upload_local_results": False,
                                              "github_event_name": context["GITHUB_EVENT_NAME"],
                                              "github_ref": context["GITHUB_REF"]})

    def test_probe_failure_does_not_enable_cache(self):
        for context in (MASTER_PUSH, {**MASTER_PUSH, "GITHUB_EVENT_NAME": "pull_request"}):
            result, rc, probed, receipt = self.configure(context, probe_status=7)
            self.assertEqual(result.returncode, 7)
            self.assertIsNotNone(probed)
            self.assertEqual(rc, "build --jobs=2\n")
            self.assertIsNone(receipt)
            self.assertNotIn(AUTH, result.stdout + result.stderr)

    def test_transport_failure_falls_back_to_local_cache_while_auth_or_redirect_fails(self):
        for error_receipt in (
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "URLError"},
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "TimeoutError"},
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "HTTPError", "status": 503},
        ):
            for context in (MASTER_PUSH, {**MASTER_PUSH, "GITHUB_EVENT_NAME": "pull_request"}):
                with self.subTest(error_receipt=error_receipt, event=context["GITHUB_EVENT_NAME"]):
                    result, rc, probed, receipt = self.configure(
                        context, probe_status=1, probe_receipt=error_receipt)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIsNotNone(probed)
                    self.assertEqual(rc, "build --jobs=2\n")
                    self.assertEqual(
                        receipt,
                        {
                            **error_receipt,
                            "enabled": False,
                            "mode": "local_only",
                            "upload_local_results": False,
                            "github_event_name": context["GITHUB_EVENT_NAME"],
                            "github_ref": context["GITHUB_REF"],
                        },
                    )
                    self.assertIn("falling back to local cache", result.stdout)
                    for secret in (AUTH, AUTH[6:], "private-test-password"):
                        self.assertNotIn(secret, result.stdout + result.stderr + rc + json.dumps(receipt))

        for fatal_receipt in (
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "ValueError"},
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "HTTPError", "status": 401},
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "HTTPError", "status": 403},
            {"authenticated": False, "endpoint": "https://cache.example", "error_type": "HTTPError", "status": 302},
        ):
            with self.subTest(fatal_receipt=fatal_receipt):
                result, rc, probed, receipt = self.configure(
                    MASTER_PUSH, probe_status=1, probe_receipt=fatal_receipt)
                self.assertEqual(result.returncode, 1)
                self.assertIsNotNone(probed)
                self.assertEqual(rc, "build --jobs=2\n")
                self.assertEqual(receipt, fatal_receipt)
                self.assertNotIn(AUTH, result.stdout + result.stderr)


if __name__ == "__main__":
    main()
