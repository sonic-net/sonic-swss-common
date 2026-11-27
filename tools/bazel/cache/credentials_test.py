"""Check the credential boundary without accessing the real cache or CI secrets."""

import base64
import email.message
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest import TestCase, main, mock
import urllib.error
import urllib.request
import urllib.response


SCRIPT = Path(__file__).with_name("credentials.py")
SPEC = importlib.util.spec_from_file_location("cache_credentials", SCRIPT)
cache_credentials = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cache_credentials)
AUTH = "Basic " + base64.b64encode(b"test-user:private-test-password").decode()


class CredentialsTests(TestCase):
    def call(self, body, auth=AUTH, args=("get",)):
        env = os.environ.copy()
        env.pop("BAZEL_REMOTE_AUTH", None)
        if auth is not None:
            env["BAZEL_REMOTE_AUTH"] = auth
        return subprocess.run([sys.executable, str(SCRIPT), *args], input=body,
                              text=True, capture_output=True, env=env, check=False)

    def assert_safe_failure(self, result):
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertEqual(result.stderr, cache_credentials.ERROR + "\n")
        self.assertNotIn(AUTH, result.stderr)
        self.assertNotIn("private-test-password", result.stderr)

    def test_get_returns_header_only_for_the_cache_origin(self):
        for suffix in ("/ac/0123", ":443/cas/0123"):
            with self.subTest(suffix=suffix):
                result = self.call(json.dumps({"uri": cache_credentials.ENDPOINT + suffix,
                                               "extension": "ignored"}))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(result.stdout), {"headers": {"Authorization": [AUTH]}})
                self.assertEqual(result.stderr, "")

    def test_get_rejects_other_origins_and_userinfo_without_disclosing_credentials(self):
        host = cache_credentials.HOST
        for uri in ("https://other.example/cas/0", "http://" + host,
                    "https://" + host + ".other.example/", "https://" + host + ":444/",
                    "https://" + host + ":invalid/", "https://" + host + ":70000/",
                    "https://test-user:private-test-password@" + host,
                    "https://@" + host, "https://" + host + "./", "//" + host):
            with self.subTest(uri=uri):
                self.assert_safe_failure(self.call(json.dumps({"uri": uri})))

    def test_missing_or_malformed_authentication_fails(self):
        request = json.dumps({"uri": cache_credentials.ENDPOINT})
        for auth in (None, "", "Bearer private-test-password", "Basic !!",
                     "Basic " + base64.b64encode(b"no-colon").decode(), AUTH + "\n"):
            with self.subTest(auth_present=auth is not None):
                self.assert_safe_failure(self.call(request, auth=auth))

    def test_malformed_requests_and_arguments_do_not_echo_the_input(self):
        for request in ("", "private-test-password", "[]", "null", "{}", '{"uri": 1}',
                        '{"uri":"https://[private-test-password"}'):
            with self.subTest(request=request):
                self.assert_safe_failure(self.call(request))
        self.assert_safe_failure(self.call("", args=("get", AUTH)))

    @mock.patch.dict(os.environ, {"BAZEL_REMOTE_AUTH": AUTH})
    def test_probe_uses_authenticated_head_and_writes_only_safe_evidence(self):
        opener = mock.Mock()
        opener.open.return_value.__enter__ = mock.Mock(return_value=mock.Mock(status=200))
        opener.open.return_value.__exit__ = mock.Mock(return_value=False)
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(cache_credentials.urllib.request, "build_opener", return_value=opener):
            output = Path(directory) / "probe.json"
            cache_credentials.probe(output)
            request = opener.open.call_args.args[0]
            self.assertEqual(request.get_method(), "HEAD")
            self.assertEqual(request.full_url, cache_credentials.EMPTY_CAS)
            self.assertEqual(request.get_header("Authorization"), AUTH)
            self.assertEqual(opener.open.call_args.kwargs, {"timeout": 10})
            self.assertEqual(json.loads(output.read_text()),
                             {"endpoint": cache_credentials.ENDPOINT, "authenticated": True, "status": 200})
            self.assertNotIn(AUTH, output.read_text())

    def test_anonymous_probe_never_reads_or_sends_credentials(self):
        for auth in (None, AUTH, "malformed-private-test-password"):
            with self.subTest(auth_present=auth is not None), tempfile.TemporaryDirectory() as directory, \
                    mock.patch.dict(os.environ, {} if auth is None else {"BAZEL_REMOTE_AUTH": auth}, clear=True), \
                    mock.patch.object(cache_credentials, "authorization") as authorization, \
                    mock.patch.object(cache_credentials.urllib.request, "build_opener") as build_opener:
                response = mock.MagicMock()
                response.__enter__.return_value.status = 200
                build_opener.return_value.open.return_value = response
                output = Path(directory) / "probe.json"
                self.assertEqual(cache_credentials.main(["probe", "--anonymous", "--output", str(output)]), 0)
                authorization.assert_not_called()
                request = build_opener.return_value.open.call_args.args[0]
                self.assertEqual(request.get_method(), "HEAD")
                self.assertEqual(request.full_url, cache_credentials.EMPTY_CAS)
                self.assertIsNone(request.get_header("Authorization"))
                self.assertEqual(json.loads(output.read_text()),
                                 {"endpoint": cache_credentials.ENDPOINT, "authenticated": False, "status": 200})
                self.assertNotIn("private-test-password", output.read_text())

    @mock.patch.dict(os.environ, {"BAZEL_REMOTE_AUTH": AUTH})
    def test_probe_rejects_redirect_before_contacting_the_new_host(self):
        requests = []

        class RedirectResponse(urllib.request.HTTPSHandler):
            def https_open(self, request):
                requests.append(request)
                headers = email.message.Message()
                headers["Location"] = "https://other.example/"
                response = urllib.response.addinfourl(io.BytesIO(), headers, request.full_url, 302)
                response.msg = "Found"
                return response

        opener = urllib.request.build_opener(cache_credentials.NoRedirects(), RedirectResponse())
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(cache_credentials.urllib.request, "build_opener", return_value=opener), \
                mock.patch.object(sys, "stderr", new_callable=io.StringIO) as stderr:
            output = Path(directory) / "probe.json"
            self.assertEqual(cache_credentials.main(["probe", "--output", str(output)]), 1)
            self.assertEqual(stderr.getvalue(), cache_credentials.ERROR + "\n")
            self.assertEqual([request.full_url for request in requests], [cache_credentials.EMPTY_CAS])
            self.assertEqual(json.loads(output.read_text()),
                             {"endpoint": cache_credentials.ENDPOINT, "authenticated": False,
                              "error_type": "HTTPError", "status": 302})

    @mock.patch.dict(os.environ, {"BAZEL_REMOTE_AUTH": AUTH})
    def test_probe_failure_replaces_stale_success_with_safe_failure_evidence(self):
        for failure in (urllib.error.HTTPError(cache_credentials.EMPTY_CAS, 401, AUTH, {}, None),
                        urllib.error.HTTPError(cache_credentials.EMPTY_CAS, 403, AUTH, {}, None),
                        urllib.error.URLError(AUTH), TimeoutError(AUTH)):
            with self.subTest(failure=type(failure).__name__), tempfile.TemporaryDirectory() as directory, \
                    mock.patch.object(cache_credentials.urllib.request, "build_opener") as build_opener, \
                    mock.patch.object(cache_credentials.time, "sleep") as sleep, \
                    mock.patch.object(sys, "stderr", new_callable=io.StringIO) as stderr:
                build_opener.return_value.open.side_effect = failure
                output = Path(directory) / "probe.json"
                output.write_text('{"authenticated": true}')
                self.assertEqual(cache_credentials.main(["probe", "--output", str(output)]), 1)
                self.assertEqual(stderr.getvalue(), cache_credentials.ERROR + "\n")
                expected = {"endpoint": cache_credentials.ENDPOINT, "authenticated": False,
                            "error_type": type(failure).__name__}
                if isinstance(failure, urllib.error.HTTPError):
                    expected["status"] = failure.code
                self.assertEqual(json.loads(output.read_text()), expected)
                self.assertNotIn(AUTH, output.read_text())
                self.assertNotIn("private-test-password", output.read_text())
                if isinstance(failure, TimeoutError):
                    self.assertEqual(build_opener.return_value.open.call_count, 3)
                    self.assertEqual(sleep.call_args_list, [mock.call(2), mock.call(4)])
                else:
                    build_opener.return_value.open.assert_called_once()
                    sleep.assert_not_called()

    @mock.patch.dict(os.environ, {"BAZEL_REMOTE_AUTH": AUTH})
    def test_probe_retries_transient_failure_then_records_success(self):
        failures = [urllib.error.HTTPError(cache_credentials.EMPTY_CAS, status, AUTH, {}, None)
                    for status in (429, 502, 503, 504)]
        failures.extend((TimeoutError(AUTH), urllib.error.URLError(TimeoutError(AUTH))))
        for failure in failures:
            with self.subTest(failure=type(failure).__name__, status=getattr(failure, "code", None)), \
                    tempfile.TemporaryDirectory() as directory, \
                    mock.patch.object(cache_credentials.urllib.request, "build_opener") as build_opener, \
                    mock.patch.object(cache_credentials.time, "sleep") as sleep, \
                    mock.patch.object(sys, "stderr", new_callable=io.StringIO) as stderr:
                response = mock.MagicMock()
                response.__enter__.return_value.status = 200
                build_opener.return_value.open.side_effect = [failure, response]
                output = Path(directory) / "probe.json"
                self.assertEqual(cache_credentials.main(["probe", "--output", str(output)]), 0)
                self.assertEqual(build_opener.return_value.open.call_count, 2)
                sleep.assert_called_once_with(2)
                self.assertEqual(stderr.getvalue(), "")
                self.assertEqual(json.loads(output.read_text()),
                                 {"endpoint": cache_credentials.ENDPOINT, "authenticated": True, "status": 200})

    @mock.patch.dict(os.environ, {"BAZEL_REMOTE_AUTH": AUTH})
    def test_probe_exhausts_three_attempts_and_records_only_the_final_failure(self):
        failures = [urllib.error.HTTPError(cache_credentials.EMPTY_CAS, status, AUTH, {}, None)
                    for status in (502, 503, 504)]
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(cache_credentials.urllib.request, "build_opener") as build_opener, \
                mock.patch.object(cache_credentials.time, "sleep") as sleep, \
                mock.patch.object(sys, "stderr", new_callable=io.StringIO) as stderr:
            build_opener.return_value.open.side_effect = failures
            output = Path(directory) / "probe.json"
            output.write_text('{"authenticated": true}')
            self.assertEqual(cache_credentials.main(["probe", "--output", str(output)]), 1)
            self.assertEqual(build_opener.return_value.open.call_count, 3)
            self.assertEqual(sleep.call_args_list, [mock.call(2), mock.call(4)])
            self.assertEqual(stderr.getvalue(), cache_credentials.ERROR + "\n")
            self.assertEqual(json.loads(output.read_text()),
                             {"endpoint": cache_credentials.ENDPOINT, "authenticated": False,
                              "error_type": "HTTPError", "status": 504})
            self.assertNotIn(AUTH, output.read_text())
            self.assertNotIn("private-test-password", output.read_text())

    @mock.patch.dict(os.environ, {}, clear=True)
    def test_probe_missing_authentication_leaves_safe_failure_evidence(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(sys, "stderr", new_callable=io.StringIO) as stderr:
            output = Path(directory) / "probe.json"
            self.assertEqual(cache_credentials.main(["probe", "--output", str(output)]), 1)
            self.assertEqual(stderr.getvalue(), cache_credentials.ERROR + "\n")
            self.assertEqual(json.loads(output.read_text()),
                             {"endpoint": cache_credentials.ENDPOINT, "authenticated": False,
                              "error_type": "ValueError"})


if __name__ == "__main__":
    main()
