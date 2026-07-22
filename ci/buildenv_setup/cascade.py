"""Resolve ``upstream-artifacts.yaml`` into concrete DEB/wheel install actions.

Two phases, so ``--dry-run`` can report without any network I/O:

* :func:`resolve_upstream_file` — pure: apply ``when:``/scope filters and
  ``{arch}``/``{debian_version}`` substitution, producing a list of
  :class:`ResolvedUpstream` (an :class:`ArtifactRef` plus resolved DEB/wheel
  glob patterns). No downloads.
* :func:`collect_bundles` — effectful: for each resolved upstream, obtain its
  bundle (a ``--upstream-staged-dir`` subdir if present, else download via
  :class:`~buildenv_setup.azp_client.AzpClient`), glob the patterns into real
  files, and recurse into the bundle's own ``build-env/`` (cascade). Dedups by
  upstream ``name`` with cycle detection.
"""

from __future__ import annotations

import glob
import logging
import os
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Set

from .azp_client import ArtifactRef, AzpClient
from .model import Context, Upstream, UpstreamFile
from .predicates import evaluate, substitute

log = logging.getLogger(__name__)


class CascadeError(RuntimeError):
    pass


@dataclass
class ResolvedDeb:
    pattern: str
    dpkg_args: List[str]
    apt_fix_broken: bool


@dataclass
class ResolvedUpstream:
    name: str
    ref: ArtifactRef
    install_env: Dict[str, str]
    debs: List[ResolvedDeb]
    wheels: List[str]              # resolved path globs
    cascade_optional: bool


@dataclass
class InstalledArtifact:
    """A concrete, fetched bundle with globbed files ready to install."""
    name: str
    bundle_dir: str
    deb_files: List[str] = field(default_factory=list)
    wheel_files: List[str] = field(default_factory=list)
    install_env: Dict[str, str] = field(default_factory=dict)
    # per-deb-file dpkg options, keyed by absolute file path
    deb_opts: Dict[str, tuple] = field(default_factory=dict)   # path -> (dpkg_args, apt_fix_broken)


def _effective_scopes(entry_scopes: Optional[List[str]], upstream_scopes: List[str]) -> List[str]:
    return entry_scopes if entry_scopes is not None else upstream_scopes


def _resolve_artifact_name(upstream: Upstream, ctx: Context) -> str:
    raw = upstream.artifact_name
    if raw is None:
        raise CascadeError(f"upstream {upstream.name!r} has no artifact_name")
    if isinstance(raw, str):
        return substitute(raw, ctx)
    if isinstance(raw, list):
        for choice in raw:
            if not isinstance(choice, dict) or "value" not in choice:
                raise CascadeError(
                    f"upstream {upstream.name!r}: artifact_name list entries need a 'value'"
                )
            if evaluate(choice.get("when"), ctx):
                return substitute(choice["value"], ctx)
        raise CascadeError(
            f"upstream {upstream.name!r}: no artifact_name choice matched context"
        )
    raise CascadeError(f"upstream {upstream.name!r}: artifact_name must be str or list")


def resolve_upstream_file(upfile: UpstreamFile, ctx: Context) -> List[ResolvedUpstream]:
    resolved: List[ResolvedUpstream] = []
    for up in upfile.upstreams:
        if not evaluate(up.when, ctx):
            continue
        if ctx.scope not in up.scopes:
            continue
        ref = ArtifactRef(
            project=up.project,
            pipeline=up.pipeline,
            artifact_name=_resolve_artifact_name(up, ctx),
            branch=up.branch or ctx.branch,
            result_filter=up.result_filter,
            run_id=up.run_id,
        )
        debs: List[ResolvedDeb] = []
        for deb in up.debs:
            if not evaluate(deb.when, ctx):
                continue
            if ctx.scope not in _effective_scopes(deb.scopes, up.scopes):
                continue
            debs.append(
                ResolvedDeb(
                    pattern=substitute(deb.path, ctx),
                    dpkg_args=deb.dpkg_args or up.dpkg_args,
                    apt_fix_broken=deb.apt_fix_broken
                    if deb.apt_fix_broken is not None
                    else up.apt_fix_broken,
                )
            )
        wheels: List[str] = []
        for wheel in up.wheels:
            if not evaluate(wheel.when, ctx):
                continue
            if ctx.scope not in _effective_scopes(wheel.scopes, up.scopes):
                continue
            wheels.append(substitute(wheel.path, ctx))
        resolved.append(
            ResolvedUpstream(
                name=up.name,
                ref=ref,
                install_env=up.install_env,
                debs=debs,
                wheels=wheels,
                cascade_optional=up.cascade_optional,
            )
        )
    return resolved


def _glob_one(bundle_dir: str, pattern: str, what: str, upstream: str) -> List[str]:
    matches = sorted(glob.glob(os.path.join(bundle_dir, pattern)))
    if not matches:
        # Fall back to a recursive search by basename: robust to differences in
        # how the artifact zip is wrapped (e.g. <artifact>/target/... vs target/...).
        matches = sorted(
            glob.glob(os.path.join(bundle_dir, "**", os.path.basename(pattern)), recursive=True)
        )
    if not matches:
        raise CascadeError(
            f"upstream {upstream!r}: no {what} matched {pattern!r} under {bundle_dir}"
        )
    return matches


def collect_bundles(
    upfile: UpstreamFile,
    ctx: Context,
    *,
    client: Optional[AzpClient],
    work_dir: str,
    staged_dir: Optional[str] = None,
    required_staged: Optional[Set[str]] = None,
    _seen: Optional[Set[str]] = None,
    _used_staged: Optional[Set[str]] = None,
) -> List[InstalledArtifact]:
    """Fetch/stage every resolved upstream, glob files, and recurse (cascade)."""
    required_staged = required_staged or set()
    seen = _seen if _seen is not None else set()
    # Thread the used-staged accumulator explicitly through the recursion so a
    # staged upstream referenced only via a NESTED bundle is still recorded as used
    # (avoids a false "required staged upstream not used" error).
    used_staged: Set[str] = _used_staged if _used_staged is not None else set()

    out: List[InstalledArtifact] = []
    for ru in resolve_upstream_file(upfile, ctx):
        if ru.name in seen:
            continue
        seen.add(ru.name)

        bundle_dir = _obtain_bundle(
            ru, client=client, work_dir=work_dir, staged_dir=staged_dir,
            required_staged=required_staged, used_staged=used_staged,
        )

        art = InstalledArtifact(name=ru.name, bundle_dir=bundle_dir, install_env=ru.install_env)
        for rdeb in ru.debs:
            for f in _glob_one(bundle_dir, rdeb.pattern, "deb", ru.name):
                art.deb_files.append(f)
                art.deb_opts[f] = (rdeb.dpkg_args, rdeb.apt_fix_broken)
        for wpat in ru.wheels:
            art.wheel_files.extend(_glob_one(bundle_dir, wpat, "wheel", ru.name))
        out.append(art)

        # Cascade: recurse into the bundle's own build-env/ if present.
        nested_up = os.path.join(bundle_dir, "build-env", "upstream-artifacts.yaml")
        if os.path.isfile(nested_up):
            from .schema import load_upstream_file
            out.extend(
                collect_bundles(
                    load_upstream_file(nested_up), ctx,
                    client=client, work_dir=work_dir, staged_dir=staged_dir,
                    required_staged=required_staged, _seen=seen, _used_staged=used_staged,
                )
            )
        elif not ru.cascade_optional:
            log.warning(
                "upstream %r bundle has no build-env/ to cascade into; "
                "treating as leaf (set cascade_optional: true to silence)", ru.name
            )

    # After the top-level pass, enforce required-staged overrides were all used.
    if _seen is None:
        unused = required_staged - used_staged
        if unused:
            raise CascadeError(
                f"required staged upstream(s) {sorted(unused)} not found/used in "
                f"--upstream-staged-dir {staged_dir!r}"
            )
    return out


def _has_glob(text: str) -> bool:
    return any(c in text for c in "*?[")


def _subpaths_for(ru: ResolvedUpstream) -> Optional[List[str]]:
    """Directory prefixes (glob-free) of this upstream's deb/wheel patterns, so
    only those subtrees are downloaded. Returns None if any pattern's directory
    part contains a glob (fall back to whole-artifact download)."""
    dirs = set()
    for rdeb in ru.debs:
        dirs.add(os.path.dirname(rdeb.pattern))
    for wpat in ru.wheels:
        dirs.add(os.path.dirname(wpat))
    dirs.discard("")
    if not dirs or any(_has_glob(d) for d in dirs):
        return None
    return sorted(dirs)


def _obtain_bundle(
    ru: ResolvedUpstream,
    *,
    client: Optional[AzpClient],
    work_dir: str,
    staged_dir: Optional[str],
    required_staged: Set[str],
    used_staged: Set[str],
) -> str:
    if staged_dir:
        candidate = os.path.join(staged_dir, ru.name)
        if os.path.isdir(candidate):
            log.info("upstream %r: using staged bundle %s", ru.name, candidate)
            used_staged.add(ru.name)
            return candidate
    if ru.name in required_staged:
        raise CascadeError(
            f"upstream {ru.name!r} is required-staged but not present in "
            f"--upstream-staged-dir {staged_dir!r}"
        )
    if client is None:
        raise CascadeError(
            f"upstream {ru.name!r} must be downloaded but no Azure DevOps client "
            f"is available (provide --upstream-staged-dir or run in a pipeline)"
        )
    log.info("upstream %r: downloading artifact %s", ru.name, ru.ref.artifact_name)
    return client.fetch_artifact(ru.ref, work_dir, subpaths=_subpaths_for(ru))
