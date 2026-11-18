from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading

import pytest
import redis
from swsscommon import swsscommon as swss
from swsscommon.redis import connection_pool
from test_redis_authentication import protected  # noqa: F401


@pytest.mark.parametrize('authenticate', [False, True])
def test_pool_explicit_authentication(protected, authenticate):
    client = redis.Redis(connection_pool=connection_pool(
        unix_socket_path=protected['socket'], db=6, authenticate=authenticate))
    assert client.ping()
    if authenticate:
        with client.pipeline(transaction=True) as transaction:
            transaction.set(b'binary\0key', b'value\0bytes')
            transaction.get(b'binary\0key')
            assert transaction.execute() == [True, b'value\0bytes']
        client.connection_pool.disconnect()
        assert client.get(b'binary\0key') == b'value\0bytes'
    else:
        with client.pipeline(transaction=True) as transaction:
            transaction.get('pool-legacy')
            assert transaction.execute() == [None]
        with pytest.raises(redis.ResponseError, match='permissions'):
            client.set('pool-legacy', 'denied')


def test_pool_rejected_credentials(protected):
    protected['secret'].write_text('wrong-test-password')
    client = redis.Redis(connection_pool=connection_pool(
        unix_socket_path=protected['socket'], authenticate=True))
    with pytest.raises(redis.AuthenticationError):
        client.ping()


def test_pool_missing_socket_is_connection_error(protected):
    client = redis.Redis(connection_pool=connection_pool(
        unix_socket_path=str(protected['work'] / 'missing.sock'), authenticate=True))
    with pytest.raises(redis.ConnectionError):
        client.ping()


def test_factory_descriptor_ownership(protected):
    before = len(os.listdir('/proc/self/fd'))
    for _ in range(8):
        fd = swss.openRedisUnixSocket(protected['socket'], 500, True)
        assert fcntl.fcntl(fd, fcntl.F_GETFD) & fcntl.FD_CLOEXEC
        assert os.get_blocking(fd)
        os.close(fd)
    assert len(os.listdir('/proc/self/fd')) == before


@contextmanager
def auth_server(protected, fragments=None):
    path = str(protected['work'] / 'pool-stalled.sock')
    listener = socket.socket(socket.AF_UNIX)
    listener.bind(path)
    listener.listen(1)
    listener.settimeout(1)
    stop, received = threading.Event(), []

    def server():
        try:
            connection, _ = listener.accept()
            with connection:
                connection.settimeout(1)
                received.append(connection.recv(1024))
                if fragments is not None:
                    for byte in fragments:
                        if stop.wait(0.06):
                            return
                        connection.sendall(bytes([byte]))
                stop.wait(3)
        except OSError:
            # A timeout can close the client before the fake reply is complete.
            pass

    thread = threading.Thread(target=server, daemon=True)
    try:
        data = json.loads(protected['policy'].read_text())
        data['unix_socket'] = path
        protected['policy'].write_text(json.dumps(data))
        thread.start()
        yield path, received
    finally:
        stop.set()
        if thread.ident is not None:
            thread.join(timeout=4)
        listener.close()


def test_pool_authentication_timeout(protected):
    with auth_server(protected) as (path, received):
        client = redis.Redis(connection_pool=connection_pool(
            unix_socket_path=path, authenticate=True,
            socket_connect_timeout=0.1, socket_timeout=1))
        with pytest.raises(redis.TimeoutError):
            client.ping()
    assert received and b'AUTH' in received[0]


@pytest.mark.parametrize('timeout_ms', [0, 100])
def test_factory_fragmented_auth_reply(protected, timeout_ms):
    with auth_server(protected, b'+OK\r\n') as (path, received):
        if timeout_ms:
            with pytest.raises(TimeoutError):
                fd = swss.openRedisUnixSocket(path, timeout_ms, True)
                os.close(fd)
        else:
            fd = swss.openRedisUnixSocket(path, timeout_ms, True)
            os.close(fd)
    assert received and b'AUTH' in received[0]


def test_authentication_requires_explicit_unix():
    with pytest.raises(ValueError, match='explicit Unix socket'):
        connection_pool(authenticate=True)


@pytest.fixture(scope='module')
def delayed_auth_read(tmp_path_factory):
    output = tmp_path_factory.mktemp('auth-delay') / 'delay.so'
    subprocess.run(['cc', '-shared', '-fPIC',
                    str(Path(__file__).with_name('redis_auth_delay_read.c')),
                    '-ldl', '-o', str(output)], check=True)
    return output


@pytest.mark.parametrize('file_name,matched,timeout_ms', [
    ('policy', True, 100), ('secret', True, 100),
    ('policy', False, 100), ('policy', True, 0),
])
def test_factory_policy_processing_deadline(protected, delayed_auth_read, file_name, matched, timeout_ms):
    if not matched:
        policy = json.loads(protected['policy'].read_text())
        policy['unix_socket'] = str(protected['work'] / 'other.sock')
        protected['policy'].write_text(json.dumps(policy))
    program = '''
import os
import sys
from swsscommon import swsscommon as swss
try:
    fd = swss.openRedisUnixSocket(sys.argv[1], int(sys.argv[2]), True)
except TimeoutError:
    if sys.argv[2] == '0':
        raise
else:
    os.close(fd)
    if sys.argv[2] != '0':
        raise AssertionError('connection returned after its AUTH deadline')
'''
    result = subprocess.run(
        [sys.executable, '-c', program, protected['socket'], str(timeout_ms)],
        env={**os.environ, 'LD_PRELOAD': str(delayed_auth_read),
             'SWSS_TEST_DELAY_AUTH_READ': str(protected[file_name])},
        capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, result.stderr
    assert 'delayed Redis authentication read' in result.stderr
