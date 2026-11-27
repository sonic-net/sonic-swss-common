#!/usr/bin/env bash
set -euo pipefail

test_binary="$1"
shift
if [[ "${test_binary}" != /* ]]; then
    test_binary="${PWD}/${test_binary}"
fi

mkdir -p "${TEST_TMPDIR}/tests/yang" "${TEST_TMPDIR}/tests/yang-missing-ref"
for fixture in "$@"; do
    fixture_group="$(basename "$(dirname "${fixture}")")"
    cp --dereference "${fixture}" "${TEST_TMPDIR}/tests/${fixture_group}/"
done
cd "${TEST_TMPDIR}"
exec "${test_binary}"
