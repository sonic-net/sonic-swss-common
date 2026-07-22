# Local-dev image for building sonic-swss-common.
#
# CI does NOT use this file: CI runs inside `container: sonic-slave-*` and invokes
# buildenv_setup directly (see .azure-pipelines/build-template.yml). This exists
# so a developer can reproduce the CI build environment locally.
#
# Layer ordering (design finding F7): copy ONLY the dependency declarations + the
# buildenv_setup tool, run setup, and do NOT copy the source. build-env/compose.yaml
# mounts the working tree at /workspace at runtime, so editing source never
# invalidates the (heavy) dependency-setup layer below.

ARG DEBIAN_VERSION=bookworm
FROM sonicdev-microsoft.azurecr.io:443/sonic-slave-${DEBIAN_VERSION}:latest

ARG DEBIAN_VERSION=bookworm
ARG BUILD_BRANCH=master

# The buildenv_setup tool (lives in this repo's ci/) + the dep declarations only.
COPY ci/ /opt/buildenv/ci/
COPY build-env/ /workspace/build-env/

# Bootstrap the tool's own runtime deps, then set up the build environment.
# --org-url points at the public SONiC Azure DevOps org for artifact download
# (in CI this comes from $SYSTEM_COLLECTIONURI instead). Set AZURE_DEVOPS_EXT_PAT,
# or use --upstream-staged-dir, if your environment needs auth.
RUN apt-get update && apt-get install -y python3-yaml python3-requests \
 && PYTHONPATH=/opt/buildenv/ci python3 -m buildenv_setup \
      --repo-dir /workspace \
      --scope build \
      --debian-version "${DEBIAN_VERSION}" \
      --branch "${BUILD_BRANCH}" \
      --org-url https://dev.azure.com/mssonic \
      --no-sudo

WORKDIR /workspace
