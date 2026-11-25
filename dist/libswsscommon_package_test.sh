#!/usr/bin/env bash
set -euo pipefail

package_archive="$1"
symbols_archive="$2"
cli_archive="$3"
cli_symbols_archive="$4"
yang_mode="$5"
database_config="$6"
package_root="${TEST_TMPDIR}/libswsscommon-package"
symbols_root="${TEST_TMPDIR}/libswsscommon-symbols"

assert_equal() {
    local description="$1" expected="$2" actual="$3"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "${description}: expected '${expected}', found '${actual}'" >&2
        exit 1
    fi
}

soname_from() {
    LC_ALL=C readelf --dynamic "$1" |
        sed -n 's/.*(SONAME).*Library soname: \[\([^]]*\)\].*/\1/p'
}

build_id_from() {
    LC_ALL=C readelf --notes "$1" | awk '/Build ID:/ { print $3; exit }'
}

validate_debug_pair() {
    local runtime_file="$1"
    local build_id debug_file debug_build_id section_file debuglink_file
    build_id="$(build_id_from "${runtime_file}")"
    if [[ ! "${build_id}" =~ ^[0-9a-f]{4,}$ ]]; then
        echo "Missing or invalid build ID in ${runtime_file}: ${build_id}" >&2
        exit 1
    fi
    debug_file="${symbols_root}/usr/lib/debug/.build-id/${build_id:0:2}/${build_id:2}.debug"
    if [[ ! -f "${debug_file}" ]]; then
        echo "Missing detached debug file for ${runtime_file}: ${debug_file}" >&2
        exit 1
    fi
    debug_build_id="$(build_id_from "${debug_file}")"
    if [[ "${debug_build_id}" != "${build_id}" ]]; then
        echo "Detached debug file build ID ${debug_build_id} does not match ${build_id}" >&2
        exit 1
    fi

    section_file="${TEST_TMPDIR}/${build_id}.sections"
    LC_ALL=C readelf --section-headers --wide "${runtime_file}" > "${section_file}"
    if grep -Eq '\.(debug_|zdebug_)' "${section_file}"; then
        echo "Runtime file still contains DWARF debug sections: ${runtime_file}" >&2
        exit 1
    fi
    LC_ALL=C readelf --section-headers --wide "${debug_file}" > "${section_file}"
    if ! grep -Eq '\.debug_info[[:space:]]' "${section_file}" ||
        ! grep -Eq '\.debug_line[[:space:]]' "${section_file}"; then
        echo "Detached file lacks source-level debug sections: ${debug_file}" >&2
        exit 1
    fi

    debuglink_file="${TEST_TMPDIR}/${build_id}.debuglink"
    objcopy --dump-section ".gnu_debuglink=${debuglink_file}" \
        "${runtime_file}" "${TEST_TMPDIR}/${build_id}.elf"
    python3 - "${debuglink_file}" "${debug_file}" <<'PY'
from pathlib import Path
import struct
import sys
import zlib

debuglink = Path(sys.argv[1]).read_bytes()
debug_file = Path(sys.argv[2])
name_end = debuglink.index(b"\0")
name = debuglink[:name_end].decode()
crc_offset = (name_end + 4) & ~3
expected_crc = struct.unpack_from("<I", debuglink, crc_offset)[0]
actual_crc = zlib.crc32(debug_file.read_bytes())
if name != debug_file.name or expected_crc != actual_crc:
    raise SystemExit(f"Invalid debug link for {debug_file}")
PY
    echo "Validated $(basename "${runtime_file}") build ID ${build_id}"
}

assert_equal "CLI runtime archive filename" "sonic-db-cli_pkg.tar" "$(basename "${cli_archive}")"

# Inspect headers before extraction, which may replace ownership with the test user.
python3 - "${package_archive}" <<'PY'
import sys
import tarfile

with tarfile.open(sys.argv[1]) as archive:
    members = {member.name.removeprefix("./"): member for member in archive}
    for name, member in members.items():
        if (member.uid, member.gid) != (0, 0):
            raise SystemExit(f"{name}: expected root ownership, found {member.uid}:{member.gid}")
        if member.issym() and member.mode != 0o777:
            raise SystemExit(f"{name}: expected symlink mode 0777, found {member.mode:04o}")
    data = [member for name, member in members.items()
            if name.startswith("usr/share/swss/") and name.endswith(".lua")]
    if not data:
        raise SystemExit("Runtime package contains no Redis Lua scripts")
    config_path = "run/redis/sonic-db/database_config.json"
    if config_path not in members:
        raise SystemExit(f"Runtime package is missing canonical {config_path}")
    data.append(members[config_path])
    for member in data:
        if not member.isfile() or member.mode != 0o644:
            raise SystemExit(f"{member.name}: expected a regular data file with mode 0644")
    if any(name == "var/run" or name.startswith("var/run/") for name in members):
        raise SystemExit("Runtime package must use /run without replacing the base /var/run link")
PY

soname="libswsscommon.so.0"
mkdir -p "${package_root}" "${symbols_root}"
tar --extract --file="${package_archive}" --directory="${package_root}"
tar --extract --file="${symbols_archive}" --directory="${symbols_root}"
tar --extract --file="${cli_archive}" --directory="${package_root}"
tar --extract --file="${cli_symbols_archive}" --directory="${symbols_root}"

# The source TAR uses the canonical path and preserves the original file bytes.
packaged_database_config="${package_root}/run/redis/sonic-db/database_config.json"
if [[ ! -f "${packaged_database_config}" || -L "${packaged_database_config}" ]] ||
    ! cmp -s "${database_config}" "${packaged_database_config}"; then
    echo "Packaged database configuration differs from common/database_config.json" >&2
    exit 1
fi

# Existing callers use /var/run through the base image's standard directory link.
mkdir -p "${package_root}/var"
ln -s ../run "${package_root}/var/run"
cmp "${database_config}" "${package_root}/var/run/redis/sonic-db/database_config.json"

if [[ -e "${package_root}/usr/lib/debug" ]]; then
    echo "Runtime package contains detached debug files" >&2
    exit 1
fi

mapfile -t runtime_libraries < <(find "${package_root}/usr/lib" -name "${soname}" -print)
if [[ "${#runtime_libraries[@]}" -ne 1 ]]; then
    echo "Expected one packaged ${soname}, found ${#runtime_libraries[@]}" >&2
    exit 1
fi

if ! resolved_library="$(realpath --canonicalize-existing "${runtime_libraries[0]}")"; then
    echo "Packaged ${soname} does not resolve to a file" >&2
    exit 1
fi
case "${resolved_library}" in
    "${package_root}"/*) ;;
    *)
        echo "Packaged ${soname} resolves outside the package: ${resolved_library}" >&2
        exit 1
        ;;
esac

packaged_soname="$(soname_from "${resolved_library}")"
if [[ "${packaged_soname}" != "${soname}" ]]; then
    echo "Packaged library SONAME '${packaged_soname}' does not match '${soname}'" >&2
    exit 1
fi

library_dir="$(dirname "${resolved_library}")"
assert_equal "Runtime library filename" "libswsscommon.so.0.0.0" "$(basename "${resolved_library}")"
assert_equal "SONAME symlink" "libswsscommon.so.0.0.0" "$(readlink "${library_dir}/${soname}")"
assert_equal "Development symlink" "${soname}" "$(readlink "${library_dir}/libswsscommon.so")"
assert_equal "Runtime library mode" "555" "$(stat --format=%a "${resolved_library}")"
assert_equal "swssloglevel mode" "755" "$(stat --format=%a "${package_root}/usr/bin/swssloglevel")"

validate_debug_pair "${resolved_library}"
validate_debug_pair "${package_root}/usr/bin/swssloglevel"
validate_debug_pair "${package_root}/usr/bin/sonic-db-cli"

cli_library="${library_dir}/libsonicdbcli.so.0.0.0"
assert_equal "CLI library SONAME" "libsonicdbcli.so.0" "$(soname_from "${cli_library}")"
assert_equal "CLI library mode" "555" "$(stat --format=%a "${cli_library}")"
assert_equal "CLI SONAME symlink" "libsonicdbcli.so.0.0.0" "$(readlink "${library_dir}/libsonicdbcli.so.0")"
assert_equal "CLI development symlink" "libsonicdbcli.so.0" "$(readlink "${library_dir}/libsonicdbcli.so")"
cli_dynamic_entries="$(LC_ALL=C readelf --dynamic "${cli_library}")"
if [[ "${cli_dynamic_entries}" != *"Shared library: [libswsscommon.so.0]"* ]]; then
    echo "libsonicdbcli does not link to the shared Common runtime" >&2
    exit 1
fi
validate_debug_pair "${cli_library}"

# The first instruction of printUsage can map to an inlined ostream header.
# Query the command entry point to verify a line in the CLI source itself.
cli_gdb_output="${TEST_TMPDIR}/libsonicdbcli-gdb.txt"
gdb --batch --nx --quiet \
    -ex "set debuginfod enabled off" \
    -ex "set debug-file-directory ${symbols_root}/usr/lib/debug" \
    -ex "file ${cli_library}" \
    -ex "info line sonic_db_cli" > "${cli_gdb_output}" 2>&1
if ! grep -Eq 'Line [0-9]+ of ".*sonic-db-cli/sonic-db-cli\.cpp"' "${cli_gdb_output}"; then
    cat "${cli_gdb_output}" >&2
    echo "GDB did not resolve the CLI library's source line" >&2
    exit 1
fi

gdb_output="${TEST_TMPDIR}/libswsscommon-gdb.txt"
gdb --batch --nx --quiet \
    -ex "set debuginfod enabled off" \
    -ex "set debug-file-directory ${symbols_root}/usr/lib/debug" \
    -ex "file ${resolved_library}" \
    -ex "info line swss::RedisReply::checkReply()" > "${gdb_output}" 2>&1
if ! grep -Eq 'Line [0-9]+ of ".*common/redisreply\.cpp"' "${gdb_output}"; then
    cat "${gdb_output}" >&2
    echo "GDB did not resolve RedisReply::checkReply() to its source line" >&2
    exit 1
fi

dynamic_entries="$(LC_ALL=C readelf --dynamic "${resolved_library}")"
symbols="$(LC_ALL=C nm --dynamic --defined-only --demangle "${resolved_library}")"
yang_symbols=(
    "swss::DefaultValueProvider::DefaultValueProvider("
    "swss::DecoratorTable::DecoratorTable("
    "swss::DecoratorSubscriberStateTable::DecoratorSubscriberStateTable("
)
case "${yang_mode}" in
    enabled)
        if [[ "${dynamic_entries}" != *"Shared library: [libyang.so.3]"* ]]; then
            echo "The YANG package must depend on libyang.so.3" >&2
            exit 1
        fi
        for symbol in "${yang_symbols[@]}"; do
            if [[ "${symbols}" != *"${symbol}"* ]]; then
                echo "The YANG package is missing exported symbol ${symbol}" >&2
                exit 1
            fi
        done
        ;;
    disabled)
        if [[ "${dynamic_entries}" == *"Shared library: [libyang.so.3]"* ]]; then
            echo "The no-YANG package must not depend on libyang.so.3" >&2
            exit 1
        fi
        for symbol in "${yang_symbols[@]}"; do
            if [[ "${symbols}" == *"${symbol}"* ]]; then
                echo "The no-YANG package unexpectedly exports ${symbol}" >&2
                exit 1
            fi
        done
        ;;
    *)
        echo "Unknown YANG mode: ${yang_mode}" >&2
        exit 1
        ;;
esac

echo "Runtime package layout, detached debug information, and YANG ${yang_mode} feature set are valid"
