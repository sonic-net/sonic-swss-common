"""Executor.dpkg_install argv construction -- ensures per-upstream install_env
(e.g. vpp's VPP_INSTALL_SKIP_SYSCTL=1) is applied on BOTH the plain and the
apt_fix_broken code paths."""
import buildenv_setup.installer as installer
from buildenv_setup.installer import Executor


def test_dpkg_plain_path_includes_env_prefix(monkeypatch):
    seen = {}
    monkeypatch.setattr(Executor, "run", lambda self, argv, env=None: seen.setdefault("argv", argv))
    ex = Executor(dry_run=False)
    ex.dpkg_install(["/d/vpp.deb"], env={"VPP_INSTALL_SKIP_SYSCTL": "1"})
    assert seen["argv"] == ["sudo", "env", "VPP_INSTALL_SKIP_SYSCTL=1", "dpkg", "-i", "/d/vpp.deb"]


def test_dpkg_fix_broken_path_includes_env_prefix(monkeypatch):
    # apt_fix_broken uses subprocess.run directly; the env prefix MUST still be in argv.
    calls = {}
    monkeypatch.setattr(installer.subprocess, "run",
                        lambda argv, **kw: calls.setdefault("argv", argv))
    ex = Executor(dry_run=False)
    ex.dpkg_install(["/d/vpp.deb", "/d/libvppinfra.deb"],
                    apt_fix_broken=True, env={"VPP_INSTALL_SKIP_SYSCTL": "1"})
    assert calls["argv"][:4] == ["sudo", "env", "VPP_INSTALL_SKIP_SYSCTL=1", "dpkg"]
    assert "/d/vpp.deb" in calls["argv"] and "/d/libvppinfra.deb" in calls["argv"]


def test_dpkg_no_env_no_prefix(monkeypatch):
    seen = {}
    monkeypatch.setattr(Executor, "run", lambda self, argv, env=None: seen.setdefault("argv", argv))
    Executor(dry_run=False).dpkg_install(["/d/libnl.deb"])
    assert seen["argv"] == ["sudo", "dpkg", "-i", "/d/libnl.deb"]


def test_dpkg_dry_run_is_noop_on_fix_broken(monkeypatch):
    called = {"subprocess": False}
    monkeypatch.setattr(installer.subprocess, "run",
                        lambda *a, **k: called.__setitem__("subprocess", True))
    # dry_run + apt_fix_broken -> falls through to Executor.run (logs only), no subprocess.
    logged = {}
    monkeypatch.setattr(Executor, "run", lambda self, argv, env=None: logged.setdefault("argv", argv))
    Executor(dry_run=True).dpkg_install(["/d/x.deb"], apt_fix_broken=True,
                                        env={"VPP_INSTALL_SKIP_SYSCTL": "1"})
    assert called["subprocess"] is False
    assert logged["argv"][:3] == ["sudo", "env", "VPP_INSTALL_SKIP_SYSCTL=1"]
