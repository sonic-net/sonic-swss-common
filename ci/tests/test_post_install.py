import os
import subprocess
from pathlib import Path

import pytest

from buildenv_setup.model import Context, PostInstall
from buildenv_setup.post_install import PostInstallError, resolve_script, select

CTX_BUILD = Context("amd64", "bookworm", "bookworm-container", "build", "master")
CTX_TEST = Context("amd64", "bookworm", "bookworm-container", "test", "master")


def _write(path, body):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(body)


def test_resolve_inline_script_wins():
    entry = PostInstall(name="x", script="echo hi")
    assert resolve_script(entry) == "echo hi"


def test_resolve_source_from_owner(tmp_path):
    owner = tmp_path / "sairedis" / "build-env"
    _write(str(owner / "s.sh"), "local-body")
    entry = PostInstall(name="x", source="s.sh", owner_build_env=str(owner))
    assert resolve_script(entry) == "local-body"


def test_resolve_source_falls_back_to_cascaded_upstream(tmp_path):
    # Consumer declares the entry but ships NO local script; the body is provided by
    # a cascaded upstream bundle's build-env/ and found by basename.
    owner = tmp_path / "sairedis" / "build-env"          # no s.sh here
    upstream = tmp_path / "swss-common" / "build-env"
    _write(str(upstream / "s.sh"), "shared-body")
    entry = PostInstall(name="x", source="s.sh", owner_build_env=str(owner))
    assert resolve_script(entry, search_dirs=[str(upstream)]) == "shared-body"


def test_resolve_local_preferred_over_cascaded(tmp_path):
    owner = tmp_path / "sairedis" / "build-env"
    upstream = tmp_path / "swss-common" / "build-env"
    _write(str(owner / "s.sh"), "local-body")
    _write(str(upstream / "s.sh"), "shared-body")
    entry = PostInstall(name="x", source="s.sh", owner_build_env=str(owner))
    assert resolve_script(entry, search_dirs=[str(upstream)]) == "local-body"


def test_resolve_missing_everywhere_raises(tmp_path):
    entry = PostInstall(name="x", source="s.sh",
                        owner_build_env=str(tmp_path / "build-env"))
    with pytest.raises(PostInstallError):
        resolve_script(entry, search_dirs=[str(tmp_path / "other")])


def test_select_scope_filter_lets_consumer_own_entry_win_at_build():
    # Mirrors the shared-redis dedup: upstream (sonic-swss-common) declares the hook
    # test-scoped; the consumer (sonic-sairedis) re-declares it [build, test]. During
    # a build-scope run the upstream entry is filtered out (so it is NOT marked seen),
    # and the consumer's own entry -- which reuses the cascaded script -- is chosen.
    upstream = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["test"], owner_build_env="/u/build-env")
    consumer = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["build", "test"], owner_build_env="/c/build-env")
    chosen = select([upstream, consumer], CTX_BUILD)   # cascaded first, then local
    assert [e.owner_build_env for e in chosen] == ["/c/build-env"]


def test_select_upstream_entry_wins_at_test_scope():
    # At test scope the upstream (cascaded-first) entry is chosen and marks the name
    # seen, so the consumer's same-named entry is skipped -- no double run.
    upstream = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["test"], owner_build_env="/u/build-env")
    consumer = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["build", "test"], owner_build_env="/c/build-env")
    chosen = select([upstream, consumer], CTX_TEST)
    assert [e.owner_build_env for e in chosen] == ["/u/build-env"]


def test_shared_redis_script_preserves_sonic_db_config_across_restart(tmp_path):
    redis_config = tmp_path / "redis.conf"
    redis_config.write_text(
        'notify-keyspace-events ""\n'
        '# unixsocket /var/run/redis/redis-server.sock\n'
        'unixsocketperm 700\n'
    )
    sonic_db_config = tmp_path / "run" / "redis" / "sonic-db" / "database_config.json"
    sonic_db_config.parent.mkdir(parents=True)
    sonic_db_config.write_text('{"DATABASES": {}}\n')

    bindir = tmp_path / "bin"
    bindir.mkdir()
    sudo = bindir / "sudo"
    sudo.write_text("#!/bin/sh\nexec \"$@\"\n")
    sudo.chmod(0o755)
    service = bindir / "service"
    service.write_text(
        "#!/bin/sh\n"
        'rm -rf "$(dirname "$SONIC_DB_CONFIG")"\n'
    )
    service.chmod(0o755)

    repo_root = Path(__file__).resolve().parents[2]
    env = os.environ.copy()
    env.update({
        "PATH": f"{bindir}:{env['PATH']}",
        "REDIS_CONFIG": str(redis_config),
        "SONIC_DB_CONFIG": str(sonic_db_config),
    })
    subprocess.run(
        ["bash", str(repo_root / "build-env" / "configure-redis-for-tests.sh")],
        env=env,
        check=True,
    )

    assert sonic_db_config.read_text() == '{"DATABASES": {}}\n'
    assert "notify-keyspace-events AKE" in redis_config.read_text()
    assert "unixsocket /var/run/redis/redis.sock" in redis_config.read_text()
    assert "unixsocketperm 777" in redis_config.read_text()
