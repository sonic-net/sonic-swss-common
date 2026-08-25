"""CLI arg surface + main() dispatch. Uses --dry-run so main() runs planner without
touching the network or the system."""
import os

import pytest

from buildenv_setup import cli


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def test_build_parser_requires_repo_dir():
    with pytest.raises(SystemExit):
        cli.build_parser().parse_args([])


def test_build_parser_defaults():
    args = cli.build_parser().parse_args(["--repo-dir", "/x"])
    assert args.scope == "build"
    assert args.arch == "amd64"
    assert args.debian_version == "bookworm"
    assert args.host_os is None
    assert args.no_sudo is False


def test_main_dry_run_ok(tmp_path, capsys):
    be = tmp_path / "build-env"
    _write(str(be / "packages" / "base.yaml"), "packages:\n  - libhiredis-dev\n")
    rc = cli.main([
        "--repo-dir", str(tmp_path), "--scope", "build",
        "--host-os", "ubuntu-22.04", "--dry-run",
    ])
    assert rc == 0
    assert "buildenv_setup dry-run" in capsys.readouterr().out


def test_main_returns_1_on_error(tmp_path):
    # No build-env/ -> planner raises -> main() catches and returns 1.
    rc = cli.main(["--repo-dir", str(tmp_path / "nope"), "--dry-run"])
    assert rc == 1


def test_main_host_os_defaults_to_container(tmp_path):
    be = tmp_path / "build-env"
    _write(str(be / "packages" / "base.yaml"), "packages:\n  - libhiredis-dev\n")
    # host_os default is <debian-version>-container; bookworm-container has no jammy
    # gating, so this still succeeds in dry-run.
    rc = cli.main(["--repo-dir", str(tmp_path), "--debian-version", "bookworm", "--dry-run"])
    assert rc == 0
