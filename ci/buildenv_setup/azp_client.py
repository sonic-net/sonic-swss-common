"""Minimal Azure DevOps REST client for fetching pipeline-artifact bundles.

Replaces the per-repo ``DownloadPipelineArtifact@2`` tasks. Given a resolved
:class:`ArtifactRef` it: resolves the pipeline definition id (by name or numeric
id), finds the build to use (a pinned run id, or the latest build on a branch
honouring ``result_filter``), gets the artifact download URL, and downloads +
extracts the zip, returning the artifact's content root.

Auth (in order): ``SYSTEM_ACCESSTOKEN`` (Bearer, the in-pipeline token) then
``AZURE_DEVOPS_EXT_PAT`` (Basic); otherwise anonymous (works for public
projects). The org URL comes from ``--org-url`` or ``SYSTEM_COLLECTIONURI``.

NOTE: bounded retries/timeouts are applied to every request; a fuller
retry/backoff policy across all network ops is an implementation-time item
(design doc F21).
"""

from __future__ import annotations

import base64
import logging
import os
import tempfile
import time
import zipfile
from dataclasses import dataclass, field
from typing import List, Optional
from urllib.parse import quote

import requests

log = logging.getLogger(__name__)

_API_VERSION = "7.0"
_TIMEOUT = 60
_RETRIES = 3


class AzpError(RuntimeError):
    pass


@dataclass
class ArtifactRef:
    project: str
    pipeline: str                       # pipeline name or numeric definition id
    artifact_name: str
    branch: str
    result_filter: List[str] = field(default_factory=lambda: ["succeeded"])
    run_id: Optional[int] = None        # explicit pin overrides branch resolution


class AzpClient:
    def __init__(self, org_url: Optional[str] = None, session: Optional[requests.Session] = None):
        self.org_url = (org_url or os.environ.get("SYSTEM_COLLECTIONURI") or "").rstrip("/")
        self.session = session or requests.Session()
        self._auth = self._build_auth()
        self._def_cache: dict = {}

    # -- auth / http ------------------------------------------------------- #
    @staticmethod
    def _build_auth() -> dict:
        token = os.environ.get("SYSTEM_ACCESSTOKEN")
        if token:
            return {"Authorization": "Bearer " + token}
        pat = os.environ.get("AZURE_DEVOPS_EXT_PAT")
        if pat:
            enc = base64.b64encode((":" + pat).encode()).decode()
            return {"Authorization": "Basic " + enc}
        log.warning("no SYSTEM_ACCESSTOKEN / AZURE_DEVOPS_EXT_PAT set; using anonymous access")
        return {}

    def _require_org(self) -> str:
        if not self.org_url:
            raise AzpError(
                "no Azure DevOps org URL (set --org-url or SYSTEM_COLLECTIONURI)"
            )
        return self.org_url

    def _get(self, url: str, params: Optional[dict] = None, stream: bool = False):
        last: Optional[Exception] = None
        for attempt in range(1, _RETRIES + 1):
            try:
                resp = self.session.get(
                    url, params=params, headers=self._auth, stream=stream, timeout=_TIMEOUT
                )
                if resp.status_code >= 500:
                    raise AzpError(f"server error {resp.status_code} for {url}")
                resp.raise_for_status()
                return resp
            except (requests.RequestException, AzpError) as exc:  # noqa: PERF203
                last = exc
                if attempt < _RETRIES:
                    backoff = 2 ** attempt
                    log.warning("GET %s failed (attempt %d/%d): %s; retrying in %ds",
                                url, attempt, _RETRIES, exc, backoff)
                    time.sleep(backoff)
        raise AzpError(f"GET {url} failed after {_RETRIES} attempts: {last}")

    # -- resolution -------------------------------------------------------- #
    def resolve_definition_id(self, project: str, pipeline: str) -> int:
        if str(pipeline).isdigit():
            return int(pipeline)
        key = (project, pipeline)
        if key in self._def_cache:
            return self._def_cache[key]
        url = f"{self._require_org()}/{project}/_apis/build/definitions"
        resp = self._get(url, {"name": pipeline, "api-version": _API_VERSION})
        values = resp.json().get("value", [])
        if not values:
            raise AzpError(f"no pipeline definition named {pipeline!r} in project {project!r}")
        def_id = values[0]["id"]
        self._def_cache[key] = def_id
        return def_id

    def resolve_build_id(self, ref: ArtifactRef) -> int:
        if ref.run_id is not None:
            return ref.run_id
        def_id = self.resolve_definition_id(ref.project, ref.pipeline)
        url = f"{self._require_org()}/{ref.project}/_apis/build/builds"
        params = {
            "definitions": def_id,
            "branchName": f"refs/heads/{ref.branch}",
            "statusFilter": "completed",
            "resultFilter": ",".join(ref.result_filter),
            "queryOrder": "finishTimeDescending",
            "$top": 1,
            "api-version": _API_VERSION,
        }
        resp = self._get(url, params)
        values = resp.json().get("value", [])
        if not values:
            raise AzpError(
                f"no completed build for pipeline {ref.pipeline!r} on branch "
                f"{ref.branch!r} (results={ref.result_filter})"
            )
        return values[0]["id"]

    def _artifact_download_url(self, project: str, build_id: int, artifact_name: str) -> str:
        url = f"{self._require_org()}/{project}/_apis/build/builds/{build_id}/artifacts"
        resp = self._get(url, {"artifactName": artifact_name, "api-version": _API_VERSION})
        download = resp.json().get("resource", {}).get("downloadUrl")
        if not download:
            raise AzpError(
                f"artifact {artifact_name!r} not found in build {build_id} (project {project})"
            )
        return download

    def _download(self, url: str, dest: str) -> None:
        resp = self._get(url, stream=True)
        with open(dest, "wb") as fh:
            for chunk in resp.iter_content(chunk_size=1 << 20):
                if chunk:
                    fh.write(chunk)

    @staticmethod
    def _content_root(extract_dir: str, artifact_name: str) -> str:
        # ADO zips wrap content in a top-level dir named after the artifact.
        named = os.path.join(extract_dir, artifact_name)
        if os.path.isdir(named):
            return named
        entries = os.listdir(extract_dir)
        if len(entries) == 1 and os.path.isdir(os.path.join(extract_dir, entries[0])):
            return os.path.join(extract_dir, entries[0])
        return extract_dir

    def fetch_artifact(
        self, ref: ArtifactRef, dest_dir: str, subpaths: Optional[List[str]] = None
    ) -> str:
        """Download + extract the artifact; return its content-root directory.

        If ``subpaths`` is given (directory prefixes with no glob chars), only
        those subtrees are downloaded via the artifact API's ``subPath`` filter
        — essential for large artifacts (e.g. sonic-buildimage.vs) where we only
        need a couple of wheels. Otherwise the whole artifact is downloaded.
        """
        build_id = self.resolve_build_id(ref)
        log.info("resolved %s -> build %d (branch %s)", ref.pipeline, build_id, ref.branch)
        url = self._artifact_download_url(ref.project, build_id, ref.artifact_name)
        os.makedirs(dest_dir, exist_ok=True)
        extract_dir = os.path.join(dest_dir, ref.artifact_name + ".extracted")
        os.makedirs(extract_dir, exist_ok=True)
        if subpaths:
            for sp in subpaths:
                self._fetch_zip(url, extract_dir, subpath=sp)
        else:
            self._fetch_zip(url, extract_dir, subpath=None)
        return self._content_root(extract_dir, ref.artifact_name)

    def _fetch_zip(self, url: str, extract_dir: str, subpath: Optional[str]) -> None:
        dl_url = url
        if subpath:
            sep = "&" if "?" in url else "?"
            dl_url = f"{url}{sep}subPath=" + quote("/" + subpath.strip("/"))
            log.info("  downloading subPath /%s", subpath.strip("/"))
        fd, zip_path = tempfile.mkstemp(suffix=".zip", dir=extract_dir)
        os.close(fd)
        try:
            self._download(dl_url, zip_path)
            with zipfile.ZipFile(zip_path) as zf:
                zf.extractall(extract_dir)
        finally:
            if os.path.exists(zip_path):
                os.remove(zip_path)
