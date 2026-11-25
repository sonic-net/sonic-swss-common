#!/bin/bash
set -euo pipefail

# Only public options go into this runner's user RC. BAZEL_REMOTE_AUTH remains
# in the client environment and is not forwarded into build or test actions.
mkdir -p artifacts/validation
upload=false
probe_args=(--anonymous)
if [[ "$(python3 -B tools/bazel/cache/policy.py)" == true && -n "${BAZEL_REMOTE_AUTH:-}" ]]; then
    upload=true
    probe_args=()
fi

probe_status=0
python3 -B tools/bazel/cache/credentials.py probe "${probe_args[@]}" \
    --output artifacts/validation/remote-cache.json || probe_status=$?

if [[ "$probe_status" -ne 0 ]]; then
    if python3 - <<'PY'
import json
import os
from pathlib import Path
import sys

receipt = Path("artifacts/validation/remote-cache.json")
if not receipt.is_file():
    sys.exit(1)
try:
    value = json.loads(receipt.read_text())
except Exception:
    sys.exit(1)
error_type = value.get("error_type")
status = value.get("status")
if error_type in ("URLError", "TimeoutError", "OSError") or (
    error_type == "HTTPError" and isinstance(status, int) and 500 <= status <= 599
):
    value.update(
        enabled=False,
        mode="local_only",
        upload_local_results=False,
        github_event_name=os.environ.get("GITHUB_EVENT_NAME"),
        github_ref=os.environ.get("GITHUB_REF"),
    )
    receipt.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    sys.exit(0)
sys.exit(1)
PY
    then
        echo 'Remote cache unavailable; falling back to local cache.'
        exit 0
    fi
    exit "$probe_status"
fi

printf '\nbuild --config=ci-remote-cache\n' >> "$HOME/.bazelrc"
if [[ "$upload" == true ]]; then
    printf 'build --config=ci-remote-cache-write\n' >> "$HOME/.bazelrc"
fi
printf 'build --remote_upload_local_results=%s\n' "$upload" >> "$HOME/.bazelrc"
python3 - "$upload" <<'PY'
import json
import os
from pathlib import Path
import sys

receipt = Path("artifacts/validation/remote-cache.json")
value = json.loads(receipt.read_text())
value.update(upload_local_results=sys.argv[1] == "true",
             github_event_name=os.environ.get("GITHUB_EVENT_NAME"),
             github_ref=os.environ.get("GITHUB_REF"))
receipt.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
PY
printf 'Remote cache enabled; upload local results: %s\n' "$upload"
