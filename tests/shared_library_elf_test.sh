#!/usr/bin/env bash
# Check the runtime contract of the uninstalled libraries, before packaging can
# remove Bazel paths. The companion C/C++ and Rust tests exercise their loading.
set -euo pipefail
export LC_ALL=C

common_library="$1"
cli_library="$2"

check_dynamic_tags() {
    local library="$1" soname="$2" dynamic
    dynamic="$(readelf --dynamic "${library}")"
    if [[ "${dynamic}" != *"Library soname: [${soname}]"* ]]; then
        echo "${library}: expected SONAME ${soname}" >&2
        exit 1
    fi
    if [[ "${dynamic}" != *"(RPATH)"* && "${dynamic}" != *"(RUNPATH)"* ]]; then
        echo "${library}: expected a search path for its uninstalled dependencies" >&2
        exit 1
    fi
    if [[ "${SWSS_EXPECT_RUNPATH:-0}" == 1 && ( "${dynamic}" == *"(RPATH)"* || "${dynamic}" != *"(RUNPATH)"* ) ]]; then
        echo "${library}: the diagnostic opt-out must use RUNPATH and contain no RPATH" >&2
        exit 1
    fi
}

check_dynamic_tags "${common_library}" libswsscommon.so.0
check_dynamic_tags "${cli_library}" libsonicdbcli.so.0

common_dynamic="$(readelf --dynamic "${common_library}")"
if [[ "${common_dynamic}" != *"Shared library: [libboost_serialization.so.1.83.0]"* ]]; then
    echo "${common_library}: expected its declared Boost serialization dependency" >&2
    exit 1
fi

common_symbols="$(nm --dynamic --defined-only --format=posix "${common_library}" | awk '{print $1}')"
# These are runtime dependencies in Make too. Embedding their static libraries
# breaks a consumer that links both Common and libnl's native shared targets.
for library in libnl-3.so.200 libnl-route-3.so.200; do
    if [[ "${common_dynamic}" != *"Shared library: [${library}]"* ]]; then
        echo "${common_library}: missing shared dependency ${library}" >&2
        exit 1
    fi
done
for symbol in nl_socket_alloc rtnl_link_alloc nfnl_ct_alloc; do
    if grep --extended-regexp --quiet "^${symbol}(@.*)?$" <<< "${common_symbols}"; then
        echo "${common_library}: must not embed libnl symbol ${symbol}" >&2
        exit 1
    fi
done
# Cover independent C API implementation files, including APIs unused by the
# small runtime test. The shared library must export them for other consumers.
for symbol in \
    SWSSString_new_c_str \
    SWSSDBConnector_new_tcp \
    SWSSConsumerStateTable_new \
    SWSSEventPublisher_new \
    SWSSZmqServer_new \
    SWSSSonicV2Connector_new; do
    if ! grep --fixed-strings --line-regexp --quiet "${symbol}" <<< "${common_symbols}"; then
        echo "${common_library}: missing public C API ${symbol}" >&2
        exit 1
    fi
done

cli_dynamic="$(readelf --dynamic "${cli_library}")"
if [[ "${cli_dynamic}" != *"Shared library: [libswsscommon.so.0]"* ]]; then
    echo "${cli_library}: expected a shared dependency on libswsscommon.so.0" >&2
    exit 1
fi
cli_symbols="$(nm --dynamic --defined-only --format=posix "${cli_library}" | awk '{print $1}')"
if grep --fixed-strings --line-regexp --quiet SWSSString_new_c_str <<< "${cli_symbols}"; then
    echo "${cli_library}: must not embed a second copy of the Common C API" >&2
    exit 1
fi

echo "Shared-library SONAMEs, search paths, C API exports and dependencies are valid"
