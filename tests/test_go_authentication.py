import os
from pathlib import Path
import shutil
import subprocess

import pytest
from test_redis_authentication import protected  # noqa: F401


def test_go_authentication(protected):
    go = shutil.which('go')
    if go is None:
        pytest.skip('Go executable is not available on PATH')
    root = Path(__file__).resolve().parents[1]
    subprocess.run(
        [go, 'test', '-tags=gcov', '-count=1', '-v', './goext/redisconn'],
        cwd=root,
        env={**os.environ, 'SWSS_AUTH_UNIX_SOCKET': protected['socket'],
             'CGO_LDFLAGS': '-L' + str(root / 'common/.libs')},
        check=True, timeout=120)
