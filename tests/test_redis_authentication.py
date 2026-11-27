"""Native opt-in tests, explicitly selected in a disposable root container.

Run with SWSS_AUTH_TEST_CONTAINER=1 and pytest --noconftest. The fixture refuses
an existing /run/redis-auth; it never overwrites an installed policy.
"""

import gc
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import socket
import threading
import time

import pytest
import redis
from swsscommon import swsscommon as swss


@pytest.fixture
def protected():
    if os.environ.get("SWSS_AUTH_TEST_CONTAINER") != "1":
        pytest.skip("requires an explicitly selected disposable container")
    assert os.geteuid() == 0 and Path("/.dockerenv").exists()
    policy_dir = Path("/run/redis-auth")
    assert not policy_dir.exists(), "refusing to replace an installed policy"
    work = Path(tempfile.mkdtemp(prefix="redis-authentication-"))
    work.chmod(0o755)
    sock = str(work / "redis.sock")
    config = work / "redis.conf"
    config.write_text(
        f"port 0\nunixsocket {sock}\nunixsocketperm 777\nsave \"\"\n"
        "notify-keyspace-events AKE\n"
        "user default on nopass ~* &* +@read +@transaction +time +ping +select +subscribe +psubscribe "
        "+unsubscribe +punsubscribe\n"
        "user test-writer on >test-only-password ~* &* +@all\n"
    )
    process = subprocess.Popen(["redis-server", str(config)], stdout=subprocess.DEVNULL,
                               stderr=subprocess.STDOUT)
    try:
        reader = redis.Redis(unix_socket_path=sock)
        for _ in range(100):
            try:
                if reader.ping():
                    break
            except redis.ConnectionError:
                time.sleep(0.02)
        else:
            pytest.fail("test Redis did not start")
        writer = redis.Redis(unix_socket_path=sock, db=6, username="test-writer",
                             password="test-only-password")
        policy_dir.mkdir(mode=0o755)
        (policy_dir / "local-system").mkdir(mode=0o755)
        secret = policy_dir / "local-system/writer.secret"
        secret.write_text("test-only-password")
        secret.chmod(0o600)
        policy = policy_dir / "local.json"
        policy.write_text(json.dumps({"username": "test-writer", "unix_socket": sock,
                                      "tcp_host": "127.0.0.1", "tcp_port": 0}))
        metadata = work / "database_config.json"
        metadata.write_text(json.dumps({
            "INSTANCES": {"redis": {"hostname": "127.0.0.1", "port": 0,
                                      "unix_socket_path": sock}},
            "DATABASES": {"STATE_DB": {"id": 6, "separator": "|", "instance": "redis"}},
        }))
        swss.SonicDBConfig.reset()
        swss.SonicDBConfig.initialize(str(metadata))
        yield {"writer": writer, "reader": reader, "secret": secret, "policy": policy,
               "metadata": metadata, "socket": sock, "work": work}
    finally:
        gc.collect()
        process.terminate()
        process.wait(timeout=5)
        shutil.rmtree(policy_dir, ignore_errors=True)
        shutil.rmtree(work)


def connect(authenticate):
    return swss.DBConnector("STATE_DB", 500, False, "", authenticate)


def test_authentication_is_explicit(protected):
    legacy = swss.DBConnector("STATE_DB", 500, False)
    with pytest.raises(RuntimeError, match="NOPERM"):
        legacy.set("legacy", "denied")
    with pytest.raises(RuntimeError, match="NOPERM"):
        connect(False).set("disabled", "denied")
    writer = swss.DBConnector("STATE_DB", 500, False, "", True)
    writer.set("explicit", "allowed")
    assert protected["writer"].get("explicit") == b"allowed"


def test_copies_and_secondary_connections(protected):
    original = connect(True)
    copies = [swss.DBConnector(original), swss.DBConnector(6, original),
              original.newConnector(500)]
    for i, copy in enumerate(copies):
        copy.set(f"copy-{i}", "allowed")
        assert copy.getDbId() == 6
    copies[2] = None
    gc.collect()
    assert original.newConnector(500).getDbName() == "STATE_DB"


@pytest.mark.parametrize("fault", ["invalid-json", "unsafe-policy", "unsafe-secret", "wrong-password"])
def test_policy_errors_do_not_affect_legacy_callers(protected, fault):
    if fault == "invalid-json":
        protected["policy"].write_text("{")
    elif fault == "unsafe-policy":
        protected["policy"].chmod(0o666)
    elif fault == "unsafe-secret":
        protected["secret"].chmod(0o644)
    else:
        protected["secret"].write_text("incorrect-test-password")
    # Successful legacy construction proves it did not inspect invalid policy.
    assert swss.DBConnector("STATE_DB", 500, False).getDbId() == 6
    before = len(os.listdir("/proc/self/fd"))
    for _ in range(8):
        with pytest.raises(RuntimeError):
            connect(True)
    gc.collect()
    assert len(os.listdir("/proc/self/fd")) == before


def test_tcp_selection_is_not_overridden(protected):
    with pytest.raises(RuntimeError, match="explicit Unix socket"):
        swss.DBConnector("STATE_DB", 500, True, "", True)
    # A legacy explicit TCP request still tries TCP even with port zero.
    with pytest.raises(RuntimeError, match="Unable to connect to redis"):
        swss.DBConnector("STATE_DB", 500, True)


def test_missing_and_unmatched_policy_keep_default_account(protected):
    protected["policy"].unlink()
    with pytest.raises(RuntimeError, match="NOPERM"):
        connect(True).set("missing-policy", "denied")
    protected["policy"].write_text(json.dumps({"username": "test-writer",
        "unix_socket": str(protected["work"] / "another.sock"),
        "tcp_host": "127.0.0.1", "tcp_port": 0}))
    with pytest.raises(RuntimeError, match="NOPERM"):
        connect(True).set("unmatched-policy", "denied")


def test_unprivileged_opt_in_remains_a_reader(protected):
    protected["writer"].set("reader-visible", "value")
    program = """
import sys
from swsscommon import swsscommon as s
s.SonicDBConfig.initialize(sys.argv[1])
c = s.DBConnector('STATE_DB', 500, False, '', True)
assert c.getDbId() == 6
assert c.get('reader-visible') == 'value'
try:
    c.set('unprivileged', 'denied')
except RuntimeError as error:
    assert 'NOPERM' in str(error)
else:
    raise AssertionError('unprivileged write succeeded')
"""
    subprocess.run(["setpriv", "--reuid=65534", "--regid=65534", "--clear-groups",
                    sys.executable, "-c", program, str(protected["metadata"])],
                   check=True, timeout=5)


@pytest.fixture
def protected_config_db(protected):
    metadata = protected["metadata"]
    config = json.loads(metadata.read_text())
    config["DATABASES"] = {"CONFIG_DB": {"id": 4, "separator": "|", "instance": "redis"}}
    metadata.write_text(json.dumps(config))
    swss.SonicDBConfig.reset()
    swss.SonicDBConfig.initialize(str(metadata))
    return protected


# The root reader run also records coverage when UID 1000 cannot write build artifacts.
@pytest.mark.parametrize("uid, authenticate", [(0, False), (1000, False), (1000, True)])
def test_logger_registration_with_read_only_redis(protected_config_db, uid, authenticate):
    program = """
import os
import sys
from swsscommon import swsscommon as s

assert os.geteuid() == int(sys.argv[2])
s.SonicDBConfig.initialize(sys.argv[1])
for component in ("logger-reader-one", "logger-reader-two"):
    s.Logger.swssOutputNotify(component, "STDERR")
    s.Logger.linkToDbNative(component, "INFO", sys.argv[3] == "True")
    assert s.Logger.getMinPrio() == s.Logger.SWSS_INFO
"""
    command = []
    if uid:
        command = ["setpriv", f"--reuid={uid}", f"--regid={uid}", "--clear-groups"]
    result = subprocess.run(
        [*command, sys.executable, "-c", program, str(protected_config_db["metadata"]),
         str(uid), str(authenticate)],
        check=True, text=True, capture_output=True, timeout=5,
    )
    assert result.stderr.count("Redis denied permission to save logger defaults") == 1
    reader = swss.DBConnector("CONFIG_DB", 500, False)
    assert not reader.exists("LOGGER|logger-reader-one")
    assert not reader.exists("LOGGER|logger-reader-two")


@pytest.fixture
def swssloglevel(protected_config_db):
    target = Path("/var/run/redis/sonic-db/database_config.json")
    target.parent.mkdir(parents=True, exist_ok=True)
    backup = target.with_name("database_config.json.loglevel-test-backup")
    assert not backup.exists() and not backup.is_symlink()
    had_original = target.exists() or target.is_symlink()
    if had_original:
        target.rename(backup)
    try:
        shutil.copyfile(protected_config_db["metadata"], target)
        binary = Path(__file__).resolve().parents[1] / "common/swssloglevel"
        yield lambda *args: subprocess.run(
            [str(binary), *args], text=True, capture_output=True, timeout=5,
        )
    finally:
        target.unlink(missing_ok=True)
        if had_original:
            backup.rename(target)


def test_swssloglevel_with_bad_writer_secret(protected_config_db, swssloglevel):
    writer = swss.DBConnector("CONFIG_DB", 500, False, "", True)
    writer.hset("LOGGER|loglevel-test", "LOGLEVEL", "INFO")
    protected_config_db["secret"].write_text("incorrect-test-password")

    for args, message in (
        ([], "Invalid loglevel value"),
        (["-c", "loglevel-test", "-l", "invalid"], "Invalid loglevel value"),
        (["-s", "-c", "SWITCH", "-l", "invalid"], "Invalid SAI loglevel value"),
        (["-a", "-c", "loglevel-test", "-l", "DEBUG"], "Invalid options provided with -a"),
        (["-p", "-d"], "-p option does not accept other options"),
    ):
        result = swssloglevel(*args)
        assert result.returncode == 1
        assert message in result.stdout and "Usage:" in result.stdout
        assert not result.stderr

    result = swssloglevel("-c", "loglevel-test", "-l", "DEBUG")
    assert result.returncode == 1
    assert "Redis writer authentication failed" in result.stderr

    result = swssloglevel("-p")
    assert result.returncode == 0
    assert result.stdout.split() == ["loglevel-test", "INFO"]


def test_notification_connections_reauthenticate(protected):
    writer = connect(True)
    consumer = swss.NotificationConsumer(writer, "auth-test")
    assert fcntl.fcntl(consumer.getFd(), fcntl.F_GETFD) & fcntl.FD_CLOEXEC
    clients = protected["writer"].execute_command("CLIENT", "LIST", "TYPE", "PUBSUB")
    assert b"user=test-writer" in clients and b"user=default" not in clients
    producer = swss.NotificationProducer(writer, "auth-test")
    producer.send("operation", "payload", swss.FieldValuePairs([]))
    selector = swss.Select()
    selector.addSelectable(consumer)
    result, _ = selector.select(1000)
    assert result == swss.Select.OBJECT
    operation, payload, _ = consumer.pop()
    assert (operation, payload) == ("operation", "payload")
    protected["writer"].execute_command("CLIENT", "KILL", "USER", "test-writer", "SKIPME", "yes")
    # Constructing a replacement subscription must authenticate a fresh socket.
    replacement = swss.NotificationConsumer(writer, "auth-test-replacement")
    assert replacement.getFd() >= 0


def test_pipeline_connections_reauthenticate(protected):
    writer = connect(True)
    producer = swss.ProducerStateTable(writer, "AUTH_PIPELINE")
    producer.set("key", swss.FieldValuePairs([("field", "value")]))
    assert producer.count() == 1


def test_authentication_timeout(protected):
    path = str(protected["work"] / "stalled.sock")
    listener = socket.socket(socket.AF_UNIX)
    listener.bind(path)
    listener.listen(1)
    listener.settimeout(1)
    stop = threading.Event()

    def stall():
        try:
            connection, _ = listener.accept()
            with connection:
                connection.settimeout(1)
                connection.recv(1024)
                stop.wait(3)
        except socket.timeout:
            pass

    thread = threading.Thread(target=stall, daemon=True)
    try:
        for file, field in [("metadata", None), ("policy", "unix_socket")]:
            data = json.loads(protected[file].read_text())
            if field:
                data[field] = path
            else:
                data["INSTANCES"]["redis"]["unix_socket_path"] = path
            protected[file].write_text(json.dumps(data))
        swss.SonicDBConfig.reset()
        swss.SonicDBConfig.initialize(str(protected["metadata"]))
        thread.start()
        started = time.monotonic()
        with pytest.raises(RuntimeError, match="authentication"):
            swss.DBConnector("STATE_DB", 100, False, "", True)
        assert time.monotonic() - started < 2
    finally:
        stop.set()
        if thread.ident is not None:
            thread.join(timeout=4)
        listener.close()


def test_subscription_authentication_errors_do_not_retry_forever(protected):
    # Run in a bounded child because NotificationConsumer historically retries all errors.
    program = """
import sys
from swsscommon import swsscommon as s
s.SonicDBConfig.initialize(sys.argv[1])
c = s.DBConnector('STATE_DB', 500, False, '', True)
open('/run/redis-auth/local-system/writer.secret', 'w').write('wrong-test-password')
try:
    s.NotificationConsumer(c, 'auth-test')
except RuntimeError:
    pass
else:
    raise AssertionError('rejected credentials were accepted')
"""
    subprocess.run([sys.executable, "-c", program, str(protected["metadata"])],
                   check=True, timeout=5)


@pytest.mark.parametrize("fault", ["policy-link", "secret-link", "directory-link", "policy-owner", "secret-owner", "directory-mode"])
def test_unsafe_authentication_inputs_are_rejected(protected, fault):
    if fault.endswith("link"):
        path = {"policy-link": protected["policy"], "secret-link": protected["secret"],
                "directory-link": protected["secret"].parent}[fault]
        moved = protected["work"] / "moved"
        path.rename(moved)
        path.symlink_to(moved, target_is_directory=fault == "directory-link")
    elif fault.endswith("owner"):
        path = protected["policy"] if fault == "policy-owner" else protected["secret"]
        os.chown(path, 65534, 65534)
    else:
        protected["policy"].parent.chmod(0o777)
    with pytest.raises(RuntimeError):
        swss.DBConnector("STATE_DB", 500, False, "", True)
    assert swss.DBConnector("STATE_DB", 500, False).getDbId() == 6
