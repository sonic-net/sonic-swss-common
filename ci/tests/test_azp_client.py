"""AzpClient: Azure DevOps REST client. Uses a fake requests.Session so no network
I/O happens; exercises auth, definition/build/artifact resolution, retry/backoff,
zip download+extract, and the zip-slip guard."""
import io
import os
import zipfile

import pytest

import buildenv_setup.azp_client as azp
from buildenv_setup.azp_client import ArtifactRef, AzpClient, AzpError


class FakeResp:
    def __init__(self, json_data=None, status=200, content=b""):
        self._json = json_data or {}
        self.status_code = status
        self._content = content

    def json(self):
        return self._json

    def raise_for_status(self):
        if self.status_code >= 400:
            import requests
            raise requests.HTTPError(f"status {self.status_code}")

    def iter_content(self, chunk_size=1):
        yield self._content


class FakeSession:
    """Returns queued responses in order (or a callable for dynamic behaviour)."""
    def __init__(self, responses):
        self._responses = list(responses)
        self.calls = []

    def get(self, url, params=None, headers=None, stream=False, timeout=None):
        self.calls.append((url, params))
        r = self._responses.pop(0)
        if callable(r):
            return r()
        return r


def _client(responses, org="https://dev.azure.com/mssonic"):
    return AzpClient(org_url=org, session=FakeSession(responses))


# -- auth -------------------------------------------------------------------- #
def test_auth_bearer_from_system_accesstoken(monkeypatch):
    monkeypatch.setenv("SYSTEM_ACCESSTOKEN", "tok")
    monkeypatch.delenv("AZURE_DEVOPS_EXT_PAT", raising=False)
    assert AzpClient(org_url="x")._auth == {"Authorization": "Bearer tok"}


def test_auth_basic_from_pat(monkeypatch):
    monkeypatch.delenv("SYSTEM_ACCESSTOKEN", raising=False)
    monkeypatch.setenv("AZURE_DEVOPS_EXT_PAT", "pat")
    auth = AzpClient(org_url="x")._auth
    assert auth["Authorization"].startswith("Basic ")


def test_auth_anonymous(monkeypatch):
    monkeypatch.delenv("SYSTEM_ACCESSTOKEN", raising=False)
    monkeypatch.delenv("AZURE_DEVOPS_EXT_PAT", raising=False)
    assert AzpClient(org_url="x")._auth == {}


def test_require_org_missing(monkeypatch):
    monkeypatch.delenv("SYSTEM_COLLECTIONURI", raising=False)
    with pytest.raises(AzpError):
        AzpClient(org_url=None)._require_org()


# -- _get retry/backoff ------------------------------------------------------ #
def test_get_retries_on_server_error(monkeypatch):
    monkeypatch.setattr(azp.time, "sleep", lambda s: None)  # no real backoff
    c = _client([FakeResp(status=500), FakeResp({"ok": 1}, status=200)])
    resp = c._get("http://x")
    assert resp.json() == {"ok": 1}


def test_get_raises_after_retries(monkeypatch):
    monkeypatch.setattr(azp.time, "sleep", lambda s: None)
    c = _client([FakeResp(status=500), FakeResp(status=500), FakeResp(status=500)])
    with pytest.raises(AzpError):
        c._get("http://x")


# -- resolve_definition_id --------------------------------------------------- #
def test_resolve_definition_numeric():
    c = _client([])
    assert c.resolve_definition_id("build", "142") == 142


def test_resolve_definition_by_name_and_cache():
    c = _client([FakeResp({"value": [{"id": 9}]})])
    assert c.resolve_definition_id("build", "Azure.sonic-swss-common") == 9
    # cached: second call makes no new request
    assert c.resolve_definition_id("build", "Azure.sonic-swss-common") == 9
    assert len(c.session.calls) == 1


def test_resolve_definition_not_found():
    c = _client([FakeResp({"value": []})])
    with pytest.raises(AzpError):
        c.resolve_definition_id("build", "nope")


# -- resolve_build_id -------------------------------------------------------- #
def test_resolve_build_id_run_id_pin():
    c = _client([])
    ref = ArtifactRef("build", "142", "art", "master", run_id=555)
    assert c.resolve_build_id(ref) == 555


def test_resolve_build_id_latest_on_branch():
    c = _client([FakeResp({"value": [{"id": 1002}]})])  # numeric pipeline -> no def lookup
    ref = ArtifactRef("build", "142", "art", "master")
    assert c.resolve_build_id(ref) == 1002


def test_resolve_build_id_none_found():
    c = _client([FakeResp({"value": []})])
    ref = ArtifactRef("build", "142", "art", "lawlee/ci-unify")
    with pytest.raises(AzpError):
        c.resolve_build_id(ref)


# -- artifact download url --------------------------------------------------- #
def test_artifact_download_url():
    c = _client([FakeResp({"resource": {"downloadUrl": "http://dl/zip"}})])
    assert c._artifact_download_url("build", 5, "art") == "http://dl/zip"


def test_artifact_download_url_missing():
    c = _client([FakeResp({"resource": {}})])
    with pytest.raises(AzpError):
        c._artifact_download_url("build", 5, "art")


# -- _content_root ----------------------------------------------------------- #
def test_content_root_named_dir(tmp_path):
    (tmp_path / "art").mkdir()
    assert AzpClient._content_root(str(tmp_path), "art") == str(tmp_path / "art")


def test_content_root_single_dir(tmp_path):
    (tmp_path / "wrapper").mkdir()
    assert AzpClient._content_root(str(tmp_path), "art") == str(tmp_path / "wrapper")


def test_content_root_fallback(tmp_path):
    (tmp_path / "a.deb").write_text("x")
    (tmp_path / "b.deb").write_text("y")
    assert AzpClient._content_root(str(tmp_path), "art") == str(tmp_path)


# -- zip-slip guard ---------------------------------------------------------- #
def _zip_bytes(members):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        for name, data in members.items():
            zf.writestr(name, data)
    return buf.getvalue()


def test_safe_extractall_ok(tmp_path):
    zpath = tmp_path / "ok.zip"
    zpath.write_bytes(_zip_bytes({"sub/file.txt": "hi"}))
    dest = tmp_path / "out"
    dest.mkdir()
    with zipfile.ZipFile(str(zpath)) as zf:
        AzpClient._safe_extractall(zf, str(dest))
    assert (dest / "sub" / "file.txt").read_text() == "hi"


def test_safe_extractall_rejects_zip_slip(tmp_path):
    zpath = tmp_path / "evil.zip"
    zpath.write_bytes(_zip_bytes({"../escape.txt": "pwn"}))
    dest = tmp_path / "out"
    dest.mkdir()
    with zipfile.ZipFile(str(zpath)) as zf:
        with pytest.raises(AzpError):
            AzpClient._safe_extractall(zf, str(dest))
    assert not (tmp_path / "escape.txt").exists()


# -- fetch_artifact (end to end, mocked download) ---------------------------- #
def test_fetch_artifact_downloads_and_extracts(tmp_path, monkeypatch):
    # resolve_build_id (numeric pipeline) + artifact url, then download writes a zip.
    c = _client([
        FakeResp({"value": [{"id": 77}]}),                       # resolve_build_id
        FakeResp({"resource": {"downloadUrl": "http://dl"}}),    # artifact url
    ])
    zip_payload = _zip_bytes({"art/target/x.deb": "deb"})

    def fake_download(url, dest):
        with open(dest, "wb") as fh:
            fh.write(zip_payload)

    monkeypatch.setattr(c, "_download", fake_download)
    ref = ArtifactRef("build", "142", "art", "master")
    root = c.fetch_artifact(ref, str(tmp_path))
    assert os.path.isdir(root)
    # content-root unwraps the single top-level dir; the deb is reachable underneath.
    found = []
    for dp, _dn, fn in os.walk(str(tmp_path)):
        found += [f for f in fn if f.endswith(".deb")]
    assert "x.deb" in found
