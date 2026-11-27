#!/usr/bin/env bash
set -euo pipefail
mode="${1:?expected prepare or trace}"
base="${COMMON_CODEQL_OUTPUT_BASE:?missing fresh CodeQL output base}"
flags=(--lockfile_mode=update --//tools/bazel:yang_modules=True
       --jobs=2 --spawn_strategy=local --action_env=PATH=/usr/bin:/bin
       --noremote_accept_cached --noremote_upload_local_results
       --disk_cache= --remote_cache= --remote_executor= --cxxopt=-nostdinc)

case "$mode" in
  prepare)
    # Build external libraries/tools and generated inputs before tracing. None
    # of these targets compiles Common's own C/C++ translation units.
    targets=(@libnl3//:libnl_3_shared @libnl3//:libnl_route_3_shared
             @libnl3//:libnl_nf_3_shared
             //common:cfg_schema_generated //pyext:swsscommon_pyy)
    bazel --batch --output_base="$base" aquery \
      "mnemonic(\"CppCompile\", deps(set(${targets[*]})))" \
      "${flags[@]}" --output=jsonproto > "$base/prepared-actions.json"
    python3 - "$base/prepared-actions.json" <<'PY'
import json, pathlib, sys
query = json.loads(pathlib.Path(sys.argv[1]).read_text())
labels = {target['id']: target['label'] for target in query.get('targets', [])}
owned = []
for action in query.get('actions', []):
    label = labels[action['targetId']]
    if label.startswith(('//', '@//', '@@//', '@@_main//', '@sonic-swss-common//', '@@sonic-swss-common//')):
        owned.append(label)
if owned:
    raise SystemExit('Preparation would compile Common before tracing: ' + ', '.join(sorted(set(owned))))
pathlib.Path('artifacts/codeql-go-inputs/prepared-dependencies.json').write_text(
    json.dumps({'external_cpp_actions': len(query.get('actions', [])), 'common_cpp_actions': 0}, indent=2) + '\n')
PY
    bazel --batch --output_base="$base" build "${flags[@]}" "${targets[@]}"
    ;;
  trace)
    # The fresh base has never compiled Common. Retain only the prepared local
    # dependency actions so CodeQL cannot enter their private loader runtimes.
    bazel --batch --output_base="$base" build "${flags[@]}" \
      //:libswsscommon //:swssloglevel //sonic-db-cli:sonic-db-cli \
      //pyext:_swsscommon //tests:codeql_test_sources \
      //tests:shared_library_runtime_test //tests/shared_netlink:consumer
    test ! -e Cargo.Bazel.lock
    git diff --exit-code HEAD -- Cargo.lock
    ;;
  *) printf 'Unknown stage: %s\n' "$mode" >&2; exit 2 ;;
esac
