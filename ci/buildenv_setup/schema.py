"""Load and validate ``build-env/`` YAML into the :mod:`buildenv_setup.model`.

Compatibility policy (design doc / F3):

* **Additive-only** — fields are only ever added, never removed, renamed, or
  repurposed. This guarantees a newer tool can always read older data.
* **Fail loud on unknown fields** — an unrecognised field is rejected with a
  clear error rather than silently ignored. In this domain an unknown field may
  encode a *required* setup step, so silently dropping it would produce a
  subtly-broken environment (a hard-to-debug failure). Failing loud instead
  surfaces tool/data version skew (and plain typos) at the introducing PR, and
  enforces the correct ordering: tool support for a field must land before any
  ``build-env/`` uses it.
"""

from __future__ import annotations

from typing import Any, Dict, List

import yaml

from .model import (
    AptSource,
    Deb,
    PackagesFile,
    Package,
    PostInstall,
    Upstream,
    UpstreamFile,
    Wheel,
)


class SchemaError(ValueError):
    pass


def _load_yaml(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as fh:
        data = yaml.safe_load(fh) or {}
    if not isinstance(data, dict):
        raise SchemaError(f"{path}: top-level YAML must be a mapping")
    return data


def _reject_unknown(where: str, mapping: Dict[str, Any], known: set) -> None:
    unknown = [k for k in mapping if k not in known]
    if unknown:
        raise SchemaError(
            f"{where}: unknown field(s) {sorted(unknown)}; the buildenv_setup tool "
            f"may need updating to support them (known fields: {sorted(known)})"
        )


def _as_entry(raw: Any) -> Dict[str, Any]:
    """Normalise a list entry that may be a bare string or a mapping."""
    if isinstance(raw, str):
        return {"name": raw}
    if isinstance(raw, dict):
        return dict(raw)
    raise SchemaError(f"expected string or mapping, got {type(raw).__name__}: {raw!r}")


def _as_str_list(value: Any, where: str, field: str) -> List[str]:
    """Validate a YAML list-of-strings field. Rejects a bare string (which
    ``list()`` would silently explode into characters) and any non-list, so a
    schema mistake fails loudly instead of producing a broken command."""
    if value is None:
        return []
    if isinstance(value, (str, bytes)) or not isinstance(value, (list, tuple)):
        raise SchemaError(
            f"{where}: {field!r} must be a list, got {type(value).__name__}: {value!r}"
        )
    return [str(v) for v in value]


def _as_str_map(value: Any, where: str, field: str) -> Dict[str, str]:
    """Validate a YAML mapping-of-strings field (e.g. install_env)."""
    if value is None:
        return {}
    if not isinstance(value, dict):
        raise SchemaError(
            f"{where}: {field!r} must be a mapping, got {type(value).__name__}: {value!r}"
        )
    return {str(k): str(v) for k, v in value.items()}


# --------------------------------------------------------------------------- #
# packages/*.yaml
# --------------------------------------------------------------------------- #

def _parse_apt_source(raw: dict, where: str) -> AptSource:
    _reject_unknown(where, raw, {"name", "list_url", "gpg_key_url", "when"})
    for req in ("name", "list_url", "gpg_key_url"):
        if req not in raw:
            raise SchemaError(f"{where}: apt_sources entry missing {req!r}")
    return AptSource(
        name=raw["name"],
        list_url=raw["list_url"],
        gpg_key_url=raw["gpg_key_url"],
        when=raw.get("when"),
    )


def _parse_package(raw: Any, where: str) -> Package:
    entry = _as_entry(raw)
    _reject_unknown(where, entry, {"name", "type", "when", "apt_source", "pip_args", "requires"})
    if "name" not in entry:
        raise SchemaError(f"{where}: package entry missing 'name'")
    ptype = entry.get("type", "apt")
    if ptype not in ("apt", "pip"):
        raise SchemaError(f"{where}: package {entry['name']!r} has invalid type {ptype!r}")
    return Package(
        name=entry["name"],
        type=ptype,
        when=entry.get("when"),
        apt_source=entry.get("apt_source"),
        pip_args=_as_str_list(entry.get("pip_args"), where, "pip_args"),
        requires=_as_str_list(entry.get("requires"), where, "requires"),
    )


def _parse_post_install(raw: dict, where: str, owner_build_env: str) -> PostInstall:
    _reject_unknown(where, raw, {"name", "source", "script", "requires", "when", "scopes"})
    if "name" not in raw:
        raise SchemaError(f"{where}: post_install entry missing 'name'")
    if bool(raw.get("source")) == bool(raw.get("script")):
        raise SchemaError(
            f"{where}: post_install {raw['name']!r} must set exactly one of source/script"
        )
    return PostInstall(
        name=raw["name"],
        source=raw.get("source"),
        script=raw.get("script"),
        requires=_as_str_list(raw.get("requires"), where, "requires"),
        when=raw.get("when"),
        scopes=_as_str_list(raw.get("scopes"), where, "scopes") or ["build"],
        owner_build_env=owner_build_env,
    )


def load_packages_file(path: str, owner_build_env: str) -> PackagesFile:
    """Parse a packages/*.yaml file. ``owner_build_env`` is the build-env/ dir
    the file belongs to (used to resolve post_install ``source:`` paths)."""
    data = _load_yaml(path)
    _reject_unknown(path, data, {"apt_sources", "packages", "post_install"})
    apt_sources = [
        _parse_apt_source(s, f"{path}:apt_sources") for s in (data.get("apt_sources") or [])
    ]
    packages = [_parse_package(p, f"{path}:packages") for p in (data.get("packages") or [])]
    post_install = [
        _parse_post_install(p, f"{path}:post_install", owner_build_env)
        for p in (data.get("post_install") or [])
    ]
    return PackagesFile(apt_sources=apt_sources, packages=packages, post_install=post_install)


# --------------------------------------------------------------------------- #
# upstream-artifacts.yaml
# --------------------------------------------------------------------------- #

def _parse_deb(raw: Any, where: str) -> Deb:
    entry = _as_entry(raw)
    # bare string uses key 'name' from _as_entry; treat it as 'path'
    if "path" not in entry and "name" in entry and len(entry) == 1:
        entry = {"path": entry["name"]}
    _reject_unknown(where, entry, {"path", "when", "dpkg_args", "apt_fix_broken", "scopes"})
    if "path" not in entry:
        raise SchemaError(f"{where}: deb entry missing 'path'")
    return Deb(
        path=entry["path"],
        when=entry.get("when"),
        dpkg_args=_as_str_list(entry.get("dpkg_args"), where, "dpkg_args"),
        apt_fix_broken=entry.get("apt_fix_broken"),
        scopes=_as_str_list(entry["scopes"], where, "scopes") if entry.get("scopes") is not None else None,
    )


def _parse_wheel(raw: Any, where: str) -> Wheel:
    entry = _as_entry(raw)
    if "path" not in entry and "name" in entry and len(entry) == 1:
        entry = {"path": entry["name"]}
    _reject_unknown(where, entry, {"path", "when", "scopes"})
    if "path" not in entry:
        raise SchemaError(f"{where}: wheel entry missing 'path'")
    return Wheel(
        path=entry["path"],
        when=entry.get("when"),
        scopes=_as_str_list(entry["scopes"], where, "scopes") if entry.get("scopes") is not None else None,
    )


def _parse_upstream(raw: dict, where: str) -> Upstream:
    _reject_unknown(
        where,
        raw,
        {
            "name", "pipeline", "project", "artifact_name", "branch", "run_id",
            "result_filter", "cascade_optional", "when", "scopes", "install_env",
            "dpkg_args", "apt_fix_broken", "debs", "wheels",
        },
    )
    for req in ("name", "pipeline"):
        if req not in raw:
            raise SchemaError(f"{where}: upstream entry missing {req!r}")
    return Upstream(
        name=raw["name"],
        pipeline=str(raw["pipeline"]),
        project=raw.get("project", "build"),
        artifact_name=raw.get("artifact_name"),
        branch=raw.get("branch"),
        run_id=raw.get("run_id"),
        result_filter=_as_str_list(raw.get("result_filter"), where, "result_filter") or ["succeeded"],
        cascade_optional=bool(raw.get("cascade_optional", False)),
        when=raw.get("when"),
        scopes=_as_str_list(raw.get("scopes"), where, "scopes") or ["build"],
        install_env=_as_str_map(raw.get("install_env"), where, "install_env"),
        dpkg_args=_as_str_list(raw.get("dpkg_args"), where, "dpkg_args"),
        apt_fix_broken=bool(raw.get("apt_fix_broken", False)),
        debs=[_parse_deb(d, f"{where}:debs") for d in (raw.get("debs") or [])],
        wheels=[_parse_wheel(w, f"{where}:wheels") for w in (raw.get("wheels") or [])],
    )


def load_upstream_file(path: str) -> UpstreamFile:
    data = _load_yaml(path)
    _reject_unknown(path, data, {"upstream"})
    upstreams = [_parse_upstream(u, f"{path}:upstream") for u in (data.get("upstream") or [])]
    names = [u.name for u in upstreams]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        raise SchemaError(f"{path}: duplicate upstream name(s): {sorted(dupes)}")
    return UpstreamFile(upstreams=upstreams)
