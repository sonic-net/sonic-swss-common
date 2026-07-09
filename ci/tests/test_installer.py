"""Executor primitives + dpkg_install argv construction.

Ensures per-upstream install_env (e.g. vpp's VPP_INSTALL_SKIP_SYSCTL=1) is applied
on BOTH the plain and the apt_fix_broken code paths, and that command execution
honours --dry-run / --no-sudo and pipefail.
"""
import subprocess

import buildenv_setup.installer as installer


def _capture_subprocess(monkeypatch):
    """Patch installer.subprocess.run to record the argv/kwargs it is called with."""
    calls = []
    monkeypatch.setattr(
        installer.subprocess, "run",
        lambda argv, **kw: calls.append((argv, kw)) or subprocess.CompletedProcess(argv, 0))
    return calls


# -- run / run_script -------------------------------------------------------- #
def test_run_executes_with_env(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).run(["echo", "hi"], env={"K": "V"})
    argv, kw = calls[0]
    assert argv == ["echo", "hi"]
    assert kw["check"] is True
    assert kw["env"]["K"] == "V"


def test_run_dry_run_is_noop(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=True).run(["echo", "hi"])
    assert calls == []


def test_run_script_uses_pipefail(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).run_script("curl x | tee y")
    argv, _ = calls[0]
    assert argv[:5] == ["bash", "-e", "-o", "pipefail", "-c"]
    assert argv[5] == "curl x | tee y"


def test_run_script_dry_run_is_noop(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=True).run_script("echo hi")
    assert calls == []


# -- package managers -------------------------------------------------------- #
def test_apt_update(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).apt_update()
    assert calls[0][0] == ["sudo", "apt-get", "update"]


def test_apt_install_empty_is_noop(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).apt_install([])
    assert calls == []


def test_apt_install_no_sudo(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False, use_sudo=False).apt_install(["a", "b"])
    assert calls[0][0] == ["apt-get", "install", "-y", "a", "b"]


def test_pip_install_with_args(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).pip_install("libyang==3.3.0", ["--no-build-isolation"])
    assert calls[0][0] == ["sudo", "pip3", "install", "--no-build-isolation", "libyang==3.3.0"]


# -- dpkg_install ------------------------------------------------------------ #
def test_dpkg_empty_is_noop(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).dpkg_install([])
    assert calls == []


def test_dpkg_plain_path_includes_env_prefix(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).dpkg_install(
        ["/d/vpp.deb"], env={"VPP_INSTALL_SKIP_SYSCTL": "1"})
    assert calls[0][0] == ["sudo", "env", "VPP_INSTALL_SKIP_SYSCTL=1", "dpkg", "-i", "/d/vpp.deb"]


def test_dpkg_fix_broken_path_includes_env_prefix(monkeypatch):
    # apt_fix_broken routes through _exec (subprocess.run); the env prefix MUST still
    # be in argv.
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).dpkg_install(
        ["/d/vpp.deb", "/d/libvppinfra.deb"],
        apt_fix_broken=True, env={"VPP_INSTALL_SKIP_SYSCTL": "1"})
    argv = calls[0][0]
    assert argv[:4] == ["sudo", "env", "VPP_INSTALL_SKIP_SYSCTL=1", "dpkg"]
    assert "/d/vpp.deb" in argv and "/d/libvppinfra.deb" in argv


def test_dpkg_fix_broken_falls_back_to_apt_f(monkeypatch):
    # First call (dpkg -i) raises CalledProcessError -> second call is apt-get -f.
    seen = []

    def fake_run(argv, **kw):
        seen.append(argv)
        if len(seen) == 1:
            raise subprocess.CalledProcessError(1, argv)
        return subprocess.CompletedProcess(argv, 0)

    monkeypatch.setattr(installer.subprocess, "run", fake_run)
    installer.Executor(dry_run=False).dpkg_install(["/d/a.deb"], apt_fix_broken=True)
    assert seen[0][:3] == ["sudo", "dpkg", "-i"]
    assert seen[1] == ["sudo", "apt-get", "install", "-y", "-f"]


def test_dpkg_no_env_no_prefix(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    installer.Executor(dry_run=False).dpkg_install(["/d/libnl.deb"])
    assert calls[0][0] == ["sudo", "dpkg", "-i", "/d/libnl.deb"]


def test_dpkg_dry_run_fix_broken_no_subprocess(monkeypatch):
    calls = _capture_subprocess(monkeypatch)
    # dry_run + apt_fix_broken -> falls through to run() (logs only), no subprocess.
    installer.Executor(dry_run=True).dpkg_install(
        ["/d/x.deb"], apt_fix_broken=True, env={"VPP_INSTALL_SKIP_SYSCTL": "1"})
    assert calls == []
