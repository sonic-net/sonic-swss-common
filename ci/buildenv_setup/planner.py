"""Orchestration: turn a repo's ``build-env/`` config into an ordered, executed
(or dry-run-printed) install plan.

Order of operations (matches today's CI and the design doc; F6 DEB-before-pip):

1. register any ``apt_sources`` referenced by selected apt packages, ``apt-get update``
2. ``apt-get install`` the selected apt packages (batched, ``requires:``-ordered)
3. ``dpkg -i`` the upstream-artifact DEBs (cascaded), honouring install_env / dpkg_args / apt_fix_broken
4. ``pip install`` the selected pip packages and the upstream wheels
5. run the selected ``post_install`` scripts
"""

from __future__ import annotations

import logging
import os
import tempfile
from collections import OrderedDict
from typing import List, Optional, Tuple

from .azp_client import AzpClient
from .cascade import collect_bundles, resolve_upstream_file
from .installer import Executor
from .model import Context, Package, PackagesFile, PostInstall
from .post_install import resolve_script, select as select_post_install
from .predicates import evaluate
from .schema import load_packages_file, load_upstream_file
from .topo import toposort

log = logging.getLogger(__name__)

_SCOPE_FILES = {
    "build": ["base.yaml", "tooling.yaml"],
    "test": ["test.yaml"],
}


def _load_local_packages(build_env: str, ctx: Context) -> List[PackagesFile]:
    files: List[PackagesFile] = []
    for name in _SCOPE_FILES.get(ctx.scope, []):
        path = os.path.join(build_env, "packages", name)
        if os.path.isfile(path):
            files.append(load_packages_file(path, build_env))
        else:
            log.debug("no %s for scope %s (skipping)", name, ctx.scope)
    return files


def _select_packages(
    files: List[PackagesFile], cascaded: List[Package], ctx: Context
) -> Tuple[List[str], List[Tuple[str, Tuple[str, ...]]]]:
    """Return (ordered apt names, ordered [(pip spec, pip_args)]) after when-filter,
    dedup, and requires: topo-sort."""
    apt: "OrderedDict[str, Package]" = OrderedDict()
    pip: "OrderedDict[str, Package]" = OrderedDict()
    for pkg in cascaded + [p for f in files for p in f.packages]:
        if not evaluate(pkg.when, ctx):
            continue
        bucket = apt if pkg.type == "apt" else pip
        bucket.setdefault(pkg.name, pkg)

    def order(bucket: "OrderedDict[str, Package]") -> List[str]:
        requires = {name: p.requires for name, p in bucket.items()}
        return toposort(list(bucket), requires)

    apt_names = order(apt)
    pip_names = order(pip)
    pip_specs = [(name, tuple(pip[name].pip_args)) for name in pip_names]
    return apt_names, pip_specs


def _select_apt_sources(files: List[PackagesFile], apt_names: List[str], ctx: Context):
    referenced = set()
    for f in files:
        for p in f.packages:
            if p.type == "apt" and p.apt_source and p.name in apt_names:
                referenced.add(p.apt_source)
    chosen = []
    seen = set()
    for f in files:
        for src in f.apt_sources:
            if src.name in referenced and src.name not in seen and evaluate(src.when, ctx):
                chosen.append(src)
                seen.add(src.name)
    return chosen


def _collect_cascaded_config(bundle_dirs: List[str], build_envs_seen: set, ctx: Context):
    """From fetched upstream bundles, collect cascading base.yaml packages +
    post_install (design: base.yaml cascades, tooling.yaml does not)."""
    packages: List[Package] = []
    post: List[PostInstall] = []
    for bundle in bundle_dirs:
        base = os.path.join(bundle, "build-env", "packages", "base.yaml")
        if bundle in build_envs_seen or not os.path.isfile(base):
            continue
        build_envs_seen.add(bundle)
        pf = load_packages_file(base, os.path.join(bundle, "build-env"))
        packages.extend(pf.packages)
        post.extend(pf.post_install)
    return packages, post


def _pip_batches(pip_specs: List[Tuple[str, Tuple[str, ...]]]):
    """Group pip specs with no extra args into one batch; others go individually."""
    plain = [name for name, args in pip_specs if not args]
    batches: List[Tuple[List[str], Tuple[str, ...]]] = []
    if plain:
        batches.append((plain, ()))
    for name, args in pip_specs:
        if args:
            batches.append(([name], args))
    return batches


def _deb_install_groups(artifacts) -> List[dict]:
    """Group every artifact's DEBs by install_env signature into single dpkg -i
    calls (see run() step 3 for rationale). Returns groups ordered so that the
    empty-install_env group (the usual library providers such as libnl/libyang3/
    libswsscommon) is installed before any special-install_env group (e.g. vpp).
    Within a group, dpkg_args are unioned and apt_fix_broken is ORed."""
    deb_groups: "OrderedDict[tuple, dict]" = OrderedDict()
    for art in artifacts:
        env_sig = tuple(sorted((art.install_env or {}).items()))
        for deb in art.deb_files:
            dpkg_args, fix = art.deb_opts.get(deb, ([], False))
            g = deb_groups.get(env_sig)
            if g is None:
                g = {"files": [], "args": [], "fix": False, "env": dict(art.install_env or {})}
                deb_groups[env_sig] = g
            g["files"].append(deb)
            for a in dpkg_args:
                if a not in g["args"]:
                    g["args"].append(a)
            g["fix"] = g["fix"] or fix
    # Empty-install_env group first (the usual library providers, installed before
    # any special-env group like vpp); all other groups keep their insertion order
    # (sorted() is stable), so inter-DEB dependencies across special-env groups are
    # not reordered.
    return [g for sig, g in sorted(deb_groups.items(), key=lambda kv: 0 if not kv[0] else 1)]


def run(
    ctx: Context,
    repo_dir: str,
    executor: Executor,
    *,
    staged_dir: Optional[str] = None,
    required_staged: Optional[set] = None,
    org_url: Optional[str] = None,
    work_dir: Optional[str] = None,
) -> None:
    build_env = os.path.join(repo_dir, "build-env")
    if not os.path.isdir(build_env):
        raise FileNotFoundError(f"no build-env/ directory under {repo_dir}")

    local_files = _load_local_packages(build_env, ctx)
    up_path = os.path.join(build_env, "upstream-artifacts.yaml")
    upfile = load_upstream_file(up_path) if os.path.isfile(up_path) else None

    # ----- dry run: report intent without any network I/O ------------------ #
    if executor.dry_run:
        apt_names, pip_specs = _select_packages(local_files, [], ctx)
        sources = _select_apt_sources(local_files, apt_names, ctx)
        resolved = resolve_upstream_file(upfile, ctx) if upfile else []
        post = select_post_install([p for f in local_files for p in f.post_install], ctx)
        _render_dry_run(ctx, sources, apt_names, pip_specs, resolved, post)
        return

    # ----- real execution -------------------------------------------------- #
    work_dir = work_dir or tempfile.mkdtemp(prefix="buildenv-")
    artifacts = []
    cascaded_pkgs: List[Package] = []
    cascaded_post: List[PostInstall] = []
    if upfile:
        client = AzpClient(org_url)
        artifacts = collect_bundles(
            upfile, ctx, client=client, work_dir=work_dir,
            staged_dir=staged_dir, required_staged=required_staged,
        )
        cascaded_pkgs, cascaded_post = _collect_cascaded_config(
            [a.bundle_dir for a in artifacts], set(), ctx
        )

    apt_names, pip_specs = _select_packages(local_files, cascaded_pkgs, ctx)
    sources = _select_apt_sources(local_files, apt_names, ctx)

    # 1. apt sources + update
    for src in sources:
        from .apt_sources import register_commands
        for cmd in register_commands(src, use_sudo=bool(executor.sudo)):
            executor.run_script(cmd)
    executor.apt_update()

    # 2. apt install
    executor.apt_install(apt_names)

    # 3. upstream DEBs (dpkg -i), before pip (F6). Group DEBs across ALL artifacts
    #    by their install_env signature and install each group in ONE dpkg -i call,
    #    so inter-DEB dependencies resolve regardless of declaration/filename order
    #    — including cross-artifact deps, e.g. libswsscommon (sonic-swss-common)
    #    depends on libnl-nf-3-200 + libyang3 (common-libs). dpkg unpacks the whole
    #    set before configuring, so it orders configuration itself. Only a differing
    #    install_env forces a separate call (e.g. vpp needs VPP_INSTALL_SKIP_SYSCTL=1
    #    during its maintainer scripts); dpkg_args are unioned and apt_fix_broken is
    #    ORed within a group. Empty-install_env groups (the usual library providers)
    #    install first, before any special-env group.
    for g in _deb_install_groups(artifacts):
        executor.dpkg_install(g["files"], dpkg_args=g["args"],
                              apt_fix_broken=g["fix"], env=g["env"])

    # 4. pip packages + wheels
    for names, args in _pip_batches(pip_specs):
        executor.run(executor.sudo + ["pip3", "install"] + list(args) + names)
    for art in artifacts:
        for wheel in art.wheel_files:
            executor.pip_install(wheel)

    # 5. post_install (cascaded upstream first, then local base, then tooling).
    #    search_dirs lets an entry's source: resolve from a cascaded upstream bundle
    #    when it isn't present locally, so a shared hook lives in one repo (the cascade
    #    root) and consumers reuse it without copying the script body.
    ordered_post = cascaded_post + [p for f in local_files for p in f.post_install]
    cascaded_build_envs = [os.path.join(a.bundle_dir, "build-env") for a in artifacts]
    for entry in select_post_install(ordered_post, ctx):
        script = resolve_script(entry, search_dirs=cascaded_build_envs)
        log.info("post_install: %s", entry.name)
        executor.run_script(script)


def _render_dry_run(ctx, sources, apt_names, pip_specs, resolved_upstreams, post):
    print(f"# buildenv_setup dry-run  (arch={ctx.arch} debian={ctx.debian_version} "
          f"host_os={ctx.host_os} scope={ctx.scope} branch={ctx.branch})")
    print("\n## apt_sources")
    for s in sources:
        print(f"  - {s.name}: {s.list_url}")
    if not sources:
        print("  (none)")
    print("\n## apt install")
    print("  " + (" ".join(apt_names) if apt_names else "(none)"))
    print("\n## pip install")
    for name, args in pip_specs:
        print(f"  - {name}" + (f"  [args: {' '.join(args)}]" if args else ""))
    if not pip_specs:
        print("  (none)")
    print("\n## upstream artifacts (would download + install)")
    for ru in resolved_upstreams:
        print(f"  - {ru.name}: pipeline={ru.ref.pipeline} project={ru.ref.project} "
              f"artifact={ru.ref.artifact_name} branch={ru.ref.branch} "
              f"results={ru.ref.result_filter}")
        for d in ru.debs:
            print(f"      deb : {d.pattern}")
        for w in ru.wheels:
            print(f"      whl : {w}")
    if not resolved_upstreams:
        print("  (none)")
    print("\n## post_install")
    for entry in post:
        kind = "script" if entry.script is not None else f"source:{entry.source}"
        print(f"  - {entry.name}  ({kind}, scopes={entry.scopes})")
    if not post:
        print("  (none)")
