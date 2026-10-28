#!/bin/bash
set -euo pipefail

if [ "$(id -u)" != 0 ]; then
    echo "Run as root to check policy ownership and ordinary-user access." >&2
    exit 1
fi

cd "$(dirname "$0")/.."
REDIS_AUTH_TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/swsscommon-redis-auth.XXXXXX")
export REDIS_AUTH_TEST_DIR
trap 'rm -rf -- "$REDIS_AUTH_TEST_DIR"' EXIT
chmod 0755 "$REDIS_AUTH_TEST_DIR"

# Reuse the schema and objects from the normal build. Its FORCE rule would
# otherwise regenerate the schema and rebuild the library for every test run.
if [ ! -f common/cfg_schema.h ]; then
    make common/cfg_schema.h
fi
make -o common/cfg_schema.h REDIS_AUTH_TEST_DIR="$REDIS_AUTH_TEST_DIR" tests/redis_auto_auth_tests
./tests/redis_auto_auth_tests --gtest_filter='RedisAutoAuth.*'

# Use the installed binding with the test library in a fresh Python process.
# The production library and packaged artifacts retain their fixed policy path.
ln -s "$PWD/tests/.libs/libswsscommon-auth-test.so" "$REDIS_AUTH_TEST_DIR/libswsscommon.so.0"
LD_LIBRARY_PATH="$REDIS_AUTH_TEST_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    PYTHONDONTWRITEBYTECODE=1 python3 -m pytest -q -o addopts= tests/test_redis_auto_auth.py
