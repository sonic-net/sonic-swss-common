"""Data model for parsed ``build-env/`` configuration.

These dataclasses are intentionally close to the YAML schema (see
``build-env/README.md`` / the design doc). Parsing/validation lives in
:mod:`buildenv_setup.schema`; this module only holds the structures and the
build :class:`Context` used for predicate evaluation and template substitution.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional


@dataclass(frozen=True)
class Context:
    """Runtime context supplied via CLI args; drives ``when:`` predicates and
    ``{arch}`` / ``{debian_version}`` / ``{host_os}`` substitution."""

    arch: str
    debian_version: str
    host_os: str
    scope: str          # "build" or "test"
    branch: str

    def as_subst(self) -> Dict[str, str]:
        return {
            "arch": self.arch,
            "debian_version": self.debian_version,
            "host_os": self.host_os,
            "branch": self.branch,
        }


@dataclass
class Package:
    name: str
    type: str = "apt"                       # "apt" | "pip"
    when: Optional[dict] = None
    apt_source: Optional[str] = None
    pip_args: List[str] = field(default_factory=list)
    requires: List[str] = field(default_factory=list)


@dataclass
class AptSource:
    name: str
    list_url: str
    gpg_key_url: str
    when: Optional[dict] = None


@dataclass
class PostInstall:
    name: str
    source: Optional[str] = None            # path relative to owning bundle's build-env/
    script: Optional[str] = None            # inline snippet (mutually exclusive with source)
    requires: List[str] = field(default_factory=list)
    when: Optional[dict] = None
    scopes: List[str] = field(default_factory=lambda: ["build"])
    # Resolved absolute path to the owning bundle's build-env/ dir; filled in by
    # the loader so post_install source: paths resolve against the right repo.
    owner_build_env: Optional[str] = None


@dataclass
class Deb:
    path: str                               # glob, relative to artifact root
    when: Optional[dict] = None
    dpkg_args: List[str] = field(default_factory=list)
    apt_fix_broken: Optional[bool] = None
    scopes: Optional[List[str]] = None      # None => inherit upstream scopes


@dataclass
class Wheel:
    path: str                               # glob, relative to artifact root
    when: Optional[dict] = None
    scopes: Optional[List[str]] = None


@dataclass
class Upstream:
    name: str
    pipeline: str
    project: str = "build"
    # artifact_name is either a plain string or a list of {when, value} choices.
    artifact_name: Any = None
    branch: Optional[str] = None            # None => use Context.branch
    run_id: Optional[int] = None            # pin a specific pipeline run (overrides branch resolution)
    result_filter: List[str] = field(default_factory=lambda: ["succeeded"])
    cascade_optional: bool = False
    when: Optional[dict] = None
    scopes: List[str] = field(default_factory=lambda: ["build"])
    install_env: Dict[str, str] = field(default_factory=dict)
    dpkg_args: List[str] = field(default_factory=list)
    apt_fix_broken: bool = False
    debs: List[Deb] = field(default_factory=list)
    wheels: List[Wheel] = field(default_factory=list)


@dataclass
class PackagesFile:
    apt_sources: List[AptSource] = field(default_factory=list)
    packages: List[Package] = field(default_factory=list)
    post_install: List[PostInstall] = field(default_factory=list)


@dataclass
class UpstreamFile:
    upstreams: List[Upstream] = field(default_factory=list)
