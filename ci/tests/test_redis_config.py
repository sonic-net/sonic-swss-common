"""The shared Redis hook must use the SONiC socket with either package's config."""
import os
from pathlib import Path
import subprocess

import pytest


@pytest.mark.parametrize("socket", [
    "# unixsocket /var/run/redis/redis-server.sock",
    "# unixsocket /run/redis.sock",
    "unixsocket /var/run/redis/redis.sock",
])
def test_redis_hook_normalizes_socket_and_checks_restart(tmp_path, socket):
    conf = tmp_path / "redis.conf"
    conf.write_text(f'{socket}\n# unixsocketperm 700\nnotify-keyspace-events ""\n')
    sudo = tmp_path / "sudo"
    sudo.write_text("""#!/bin/bash
set -e
case "$1" in
    sed)
        args=("$@")
        args[${#args[@]}-1]="$REDIS_TEST_CONFIG"
        exec "${args[@]}"
        ;;
    install|service) echo "$*" >> "$REDIS_TEST_LOG" ;;
    redis-cli) echo "${REDIS_TEST_PING:-PONG}" ;;
    *) exit 1 ;;
esac
""")
    sudo.chmod(0o755)
    hook = Path(__file__).resolve().parents[2] / "build-env" / "configure-redis-for-tests.sh"
    env = dict(os.environ, PATH=f"{tmp_path}:{os.environ['PATH']}",
               REDIS_TEST_CONFIG=str(conf), REDIS_TEST_LOG=str(tmp_path / "calls"))
    subprocess.run(["bash", str(hook)], env=env, check=True, capture_output=True)
    assert conf.read_text().splitlines() == [
        "unixsocket /var/run/redis/redis.sock",
        "unixsocketperm 777",
        "notify-keyspace-events AKE",
    ]
    assert "service redis-server restart" in (tmp_path / "calls").read_text()
    env["REDIS_TEST_PING"] = "LOADING"
    failed = subprocess.run(["bash", str(hook)], env=env, capture_output=True)
    assert failed.returncode != 0
