"""Check cache seed policy and anonymous validation readback."""

import os
from pathlib import Path
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



if __name__ == "__main__":
    main()
