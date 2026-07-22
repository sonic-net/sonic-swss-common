#!/bin/bash
#
# Canonical build for sonic-swss-common.
#
# Single source of truth for "how to build this repo", used by BOTH CI
# (.azure-pipelines/build-template.yml) and local dev (build-env/compose.yaml).
# Must NOT depend on CI-only environment variables.
#
# The build environment (apt/pip deps + upstream libyang/yang artifacts) is set
# up beforehand by `buildenv_setup` (see build-env/README.md); this script only
# runs the actual package build.
set -ex

# Run from the repo root regardless of where we were invoked.
cd "$(dirname "$0")/.."

# Coverage-enabled build (matches build-template.yml for all arches today).
rm -f ../*.deb || true
./autogen.sh
DEB_CONFIGURE_EXTRA_FLAGS='--enable-code-coverage' \
  DEB_CXXFLAGS_APPEND="-coverage -fprofile-abs-path" \
  DEB_LDFLAGS_APPEND="-coverage -fprofile-abs-path" \
  dpkg-buildpackage -Pnopython2 -us -uc -b -j"$(nproc)"

# Collect the built .debs at the repo root (where CI publishes from).
mv ../*.deb .
