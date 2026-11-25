#!/usr/bin/env bash
# Read-only phase snapshots; filesystem rows and directory totals can overlap.
set -euo pipefail

stage=${1:?Usage: record_disk_usage.sh STAGE}
workspace=${GITHUB_WORKSPACE:-$PWD}
printf '\nDisk usage: %s at %s\n' "$stage" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"

printf '\nFilesystem bytes (capacity, used, available):\n'
df -B1 -P "$workspace" "$HOME"
printf '\nFilesystem inodes:\n'
df -i -P "$workspace" "$HOME"

# These locations match the pinned setup-bazel action's container defaults.
# du does not follow the workspace's bazel-* symlinks or cross filesystems.
paths=()
for path in "$workspace" "$HOME/.bazel" "$HOME/.cache/bazel-disk" \
    "$HOME/.cache/bazel-repo" "$HOME/.cache/bazelisk"; do
    if [[ -d "$path" ]]; then
        paths+=("$path")
    fi
done
printf '\nDirectory allocated bytes (bounded to 120 seconds):\n'
timeout 120 du -x -B1 --max-depth=1 "${paths[@]}"
