"""RDB CLI tests run in the disposable native-authentication container."""
import json
from pathlib import Path
import shutil
import socket
import subprocess
import threading
import time

import pytest
import redis

from test_cli_authentication import cli, protected  # noqa: F401


def wait_for_redis(client):
    for _ in range(100):
        try:
            if client.ping():
                return
        except redis.ConnectionError:
            time.sleep(0.02)
    pytest.fail("test Redis did not start")


@pytest.fixture
def rdb(protected, cli):
    # SYNC needs a writable Redis directory for its background snapshot.
    path = str(protected["work"] / "rdb.sock")
    config = protected["work"] / "rdb.conf"
    config.write_text((protected["work"] / "redis.conf").read_text().replace(protected["socket"], path))
    server = subprocess.Popen(["redis-server", str(config)], cwd=protected["work"],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        writer = redis.Redis(unix_socket_path=path, db=6,
                             username="test-writer", password="test-only-password")
        wait_for_redis(writer)
        data = json.loads(protected["metadata"].read_text())
        data["INSTANCES"]["redis"]["unix_socket_path"] = path
        metadata = protected["work"] / "rdb-database.json"
        metadata.write_text(json.dumps(data))
        Path("/var/run/redis/sonic-db/database_config.json").write_text(json.dumps(data))
        policy = json.loads(protected["policy"].read_text())
        policy["unix_socket"] = path
        protected["policy"].write_text(json.dumps(policy))
        yield {**protected, "writer": writer, "socket": path, "metadata": metadata}
    finally:
        server.terminate()
        server.wait(timeout=5)


def check_snapshot(path):
    assert path.read_bytes().startswith(b"REDIS")
    assert path.stat().st_mode & 0o777 == 0o600
    subprocess.run(["redis-check-rdb", str(path)], check=True, capture_output=True)


def test_cli_rdb_exports_whole_instance(rdb, cli):
    rdb["writer"].set(b"rdb\0key", b"binary\0value")
    other = redis.Redis(unix_socket_path=rdb["socket"], db=0,
                        username="test-writer", password="test-only-password")
    other.set("other-database", "included")
    output = rdb["work"] / "snapshot with spaces.rdb"
    result = cli("-s", "--authenticate", "--rdb", str(output), "STATE_DB")
    assert result.returncode == 0, result.stderr
    check_snapshot(output)

    directory = rdb["work"] / "restore"
    directory.mkdir()
    shutil.copyfile(output, directory / "dump.rdb")
    restore_socket = str(directory / "redis.sock")
    server = subprocess.Popen([
        "redis-server", "--port", "0", "--unixsocket", restore_socket,
        "--dir", str(directory), "--save", "", "--appendonly", "no",
    ], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        restored = redis.Redis(unix_socket_path=restore_socket, db=6)
        wait_for_redis(restored)
        assert restored.get(b"rdb\0key") == b"binary\0value"
        assert redis.Redis(unix_socket_path=restore_socket).get("other-database") == b"included"
    finally:
        server.terminate()
        server.wait(timeout=5)


@pytest.mark.parametrize("fault", ["no-authentication", "bad-password", "tcp-not-selected"])
def test_cli_rdb_failure_preserves_destination(rdb, cli, fault):
    output = rdb["work"] / "existing.rdb"
    output.write_bytes(b"original")
    args = ["-s", "--authenticate"]
    if fault == "no-authentication":
        args = ["-s"]
    elif fault == "bad-password":
        rdb["secret"].write_text("incorrect-test-password")
    else:
        args = ["--authenticate"]
    result = cli(*args, "--rdb", str(output), "STATE_DB")
    assert result.returncode != 0
    assert output.read_bytes() == b"original"
    assert list(output.parent.glob("existing.rdb.*")) == []


@pytest.mark.parametrize("arguments", [
    ["--rdb"], ["--rdb", ""], ["--rdb", "OUTPUT"],
    ["--rdb", "OUTPUT", "STATE_DB", "PING"],
    ["--json", "--rdb", "OUTPUT", "STATE_DB"],
    ["--rdb", "OUTPUT", "UNKNOWN_DB"],
])
def test_cli_rdb_rejects_invalid_arguments(rdb, cli, arguments):
    output = rdb["work"] / "invalid.rdb"
    result = cli(*(str(output) if arg == "OUTPUT" else arg for arg in arguments))
    assert result.returncode != 0
    assert not output.exists()


def test_cli_rdb_legacy_tcp_uses_configured_endpoint(rdb, cli):
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.2", 0))
        port = reserve.getsockname()[1]
    server = subprocess.Popen([
        "redis-server", "--bind", "127.0.0.2", "--port", str(port),
        "--dir", str(rdb["work"]), "--save", "", "--appendonly", "no",
    ], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        client = redis.Redis(host="127.0.0.2", port=port)
        wait_for_redis(client)
        client.set("legacy-tcp", "included")
        metadata = Path("/var/run/redis/sonic-db/database_config.json")
        data = json.loads(metadata.read_text())
        data["INSTANCES"]["redis"].update(hostname="127.0.0.2", port=port)
        metadata.write_text(json.dumps(data))
        output = rdb["work"] / "legacy.rdb"
        result = cli("--rdb", str(output), "STATE_DB")
        assert result.returncode == 0, result.stderr
        check_snapshot(output)
    finally:
        server.terminate()
        server.wait(timeout=5)


def test_cli_rdb_namespace_selects_unix_socket(rdb, cli):
    directory = Path("/var/run/redis/sonic-db")
    global_config = directory / "database_global.json"
    backup = directory / "database_global.json.rdb-test-backup"
    namespace_file = directory / "rdb-namespace.json"
    assert not backup.exists() and not namespace_file.exists()
    had_original = global_config.exists() or global_config.is_symlink()
    if had_original:
        global_config.rename(backup)
    try:
        namespace_file.write_text(rdb["metadata"].read_text())
        global_config.write_text(json.dumps({
            "INCLUDES": [{"namespace": "asic0", "include": namespace_file.name}],
            "VERSION": "1.0",
        }))
        output = rdb["work"] / "namespace.rdb"
        result = cli("-n", "asic0", "--authenticate", "--rdb", str(output), "STATE_DB")
        assert result.returncode == 0, result.stderr
        check_snapshot(output)
    finally:
        global_config.unlink(missing_ok=True)
        namespace_file.unlink(missing_ok=True)
        if had_original:
            backup.rename(global_config)


@pytest.mark.parametrize("header", [b"$100\r\nshort", b"$EOF:unsupported\r\n"])
def test_cli_bad_rdb_stream_preserves_destination(rdb, cli, header):
    path = str(rdb["work"] / "bad-rdb.sock")
    listener = socket.socket(socket.AF_UNIX)
    listener.bind(path)
    listener.listen(1)
    listener.settimeout(2)
    received = []

    def server():
        connection, _ = listener.accept()
        with connection:
            received.append(connection.recv(1024))
            connection.sendall(header)

    thread = threading.Thread(target=server, daemon=True)
    metadata = Path("/var/run/redis/sonic-db/database_config.json")
    data = json.loads(metadata.read_text())
    data["INSTANCES"]["redis"]["unix_socket_path"] = path
    metadata.write_text(json.dumps(data))
    output = rdb["work"] / "existing.rdb"
    output.write_bytes(b"original")
    thread.start()
    try:
        result = cli("-s", "--rdb", str(output), "STATE_DB")
        assert result.returncode != 0
        assert output.read_bytes() == b"original"
        assert list(output.parent.glob("existing.rdb.*")) == []
    finally:
        thread.join(timeout=3)
        listener.close()
    assert received == [b"SYNC\r\n"]
