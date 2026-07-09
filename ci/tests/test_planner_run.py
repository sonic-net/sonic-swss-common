"""planner.run() orchestration end-to-end, with a fake Executor and staged upstream
bundles (no network). Covers dry-run rendering, the real install sequence
(apt/dpkg/pip/post_install), apt_source selection, and the cascade collection."""
import os

import pytest

from buildenv_setup.model import Context
from buildenv_setup import planner


class FakeExecutor:
    def __init__(self, dry_run=False, use_sudo=True):
        self.dry_run = dry_run
        self.sudo = ["sudo"] if use_sudo else []
        self.calls = []

    def run(self, argv, env=None):
        self.calls.append(("run", list(argv), env))

    def run_script(self, script, env=None):
        self.calls.append(("script", script))

    def apt_update(self):
        self.calls.append(("apt_update",))

    def apt_install(self, pkgs):
        self.calls.append(("apt_install", list(pkgs)))

    def pip_install(self, spec, args=None):
        self.calls.append(("pip", spec, list(args or [])))

    def dpkg_install(self, files, dpkg_args=None, apt_fix_broken=False, env=None):
        self.calls.append(("dpkg", list(files), dict(env or {})))


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


@pytest.fixture
def repo(tmp_path):
    """A minimal repo build-env/ plus a staged upstream bundle."""
    be = tmp_path / "repo" / "build-env"
    _write(str(be / "packages" / "base.yaml"), """
packages:
  - libhiredis-dev
  - { name: redis-server }
  - name: libyang==3.3.0
    type: pip
    pip_args: [--no-build-isolation]
    requires: [libyang3]
post_install:
  - name: setup-redis
    source: redis.sh
    scopes: [build]
""")
    _write(str(be / "packages" / "tooling.yaml"), """
packages:
  - { name: cmake, type: apt }
""")
    _write(str(be / "upstream-artifacts.yaml"), """
upstream:
  - name: common-libs
    pipeline: Azure.sonic-buildimage.common_libs
    artifact_name: common-lib
    cascade_optional: true
    debs:
      - 'target/debs/bookworm/libyang3_*.deb'
""")
    _write(str(be / "redis.sh"), "echo configuring redis\n")

    # Staged bundle for the "common-libs" upstream with a matching deb.
    staged = tmp_path / "staged"
    _write(str(staged / "common-libs" / "target" / "debs" / "bookworm" / "libyang3_3.deb"), "x")
    return {"repo_dir": str(tmp_path / "repo"), "staged": str(staged)}


CTX = Context("amd64", "bookworm", "bookworm-container", "build", "master")


def test_run_dry_run_prints_plan(repo, capsys):
    ex = FakeExecutor(dry_run=True)
    planner.run(CTX, repo["repo_dir"], ex, staged_dir=repo["staged"])
    out = capsys.readouterr().out
    assert "buildenv_setup dry-run" in out
    assert "libhiredis-dev" in out
    assert "common-libs" in out
    assert "setup-redis" in out
    assert ex.calls == []                      # dry-run executes nothing


def test_run_real_sequence(repo):
    ex = FakeExecutor(dry_run=False)
    planner.run(CTX, repo["repo_dir"], ex, staged_dir=repo["staged"])
    kinds = [c[0] for c in ex.calls]
    assert "apt_update" in kinds
    # apt install includes the base + tooling apt packages
    apt = next(c for c in ex.calls if c[0] == "apt_install")
    assert "libhiredis-dev" in apt[1] and "redis-server" in apt[1] and "cmake" in apt[1]
    # the staged upstream deb is dpkg-installed
    dpkg = [c for c in ex.calls if c[0] == "dpkg"]
    assert any(any(f.endswith("libyang3_3.deb") for f in c[1]) for c in dpkg)
    # pip install of libyang happened
    assert any(c[0] == "run" and "libyang==3.3.0" in c[1] for c in ex.calls) \
        or any(c[0] == "pip" and "libyang==3.3.0" in c[1] for c in ex.calls)
    # post_install script ran
    assert any(c[0] == "script" and "configuring redis" in c[1] for c in ex.calls)


def test_run_no_build_env_raises(tmp_path):
    with pytest.raises(FileNotFoundError):
        planner.run(CTX, str(tmp_path / "empty"), FakeExecutor())


def test_select_apt_sources_only_when_referenced():
    from buildenv_setup.model import AptSource, Package, PackagesFile
    src = AptSource(name="dotnet", list_url="l", gpg_key_url="g")
    files = [PackagesFile(
        apt_sources=[src],
        packages=[Package(name="dotnet-sdk", type="apt", apt_source="dotnet")],
    )]
    chosen = planner._select_apt_sources(files, ["dotnet-sdk"], CTX)
    assert [s.name for s in chosen] == ["dotnet"]
    # not referenced -> not chosen
    assert planner._select_apt_sources(files, [], CTX) == []
