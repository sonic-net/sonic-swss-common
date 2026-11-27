"""Verify the host gate rejects obsolete packages and unexpected Redis versions."""
import os
from pathlib import Path
import subprocess

import pytest


@pytest.mark.parametrize("obsolete", [
    "", "python3-pip", "lua-bitop", "lua-cjson", "liblua5.1-0",
])
def test_host_gate_checks_dpkg_not_path(tmp_path, obsolete):
    dpkg = tmp_path / "dpkg-query"
    dpkg.write_text("""#!/bin/bash
case "${@: -1}" in
    redis-server|redis-tools) echo "installed ${TEST_REDIS_VERSION:-6:7.2.17-1rl1~jammy1}" ;;
    "$TEST_OBSOLETE") echo installed ;;
    *) exit 1 ;;
esac
""")
    dpkg.chmod(0o755)
    script = Path(__file__).resolve().parents[1] / "buildenv_setup" / "jammy-host.sh"
    env = dict(os.environ, PATH=f"{tmp_path}:{os.environ['PATH']}",
               TEST_OBSOLETE=obsolete)
    command = [
        "bash", "-c",
        'source() { ID=ubuntu; VERSION_ID=22.04; }; export -f source; exec bash "$1" verify',
        "host-test", str(script),
    ]
    result = subprocess.run(command, env=env, capture_output=True, text=True)
    assert (result.returncode == 0) == (not obsolete)
    if obsolete:
        assert f"reintroduced: {obsolete}" in result.stderr
    else:
        env["TEST_REDIS_VERSION"] = "5:6.0.16-1ubuntu1"
        result = subprocess.run(command, env=env, capture_output=True, text=True)
        assert result.returncode != 0
        assert "Unexpected redis-server" in result.stderr
