# build-env/

Declarative build-environment configuration for sonic-swss-common, consumed by
the shared [`buildenv_setup`](../ci/README.md) tool. This is the single source of
truth for "how to build this repo": what dependencies it needs, which upstream
artifacts it consumes, and the build command itself.

## Contents

| Path | Purpose | Cascades to downstream? |
|------|---------|-------------------------|
| `packages/base.yaml` | apt/pip packages needed to build **and link against** libswsscommon; redis + its test-config | **Yes** |
| `packages/tooling.yaml` | Build-stage-only tooling (python test deps, yang-models dir) | No |
| `upstream-artifacts.yaml` | upstream DEBs/wheels to fetch (libyang from common-libs; sonic-yang wheels from sonic-buildimage) | Yes |
| `configure-redis-for-tests.sh` | redis test-config, run via a `post_install` hook | (travels with base.yaml) |
| `build.sh` | canonical build command (autogen + dpkg-buildpackage), used by CI **and** local dev | — |
| `Dockerfile`, `compose.yaml` | local-dev image (CI does not use these) | — |

## How CI uses it

`.azure-pipelines/build-template.yml` runs, inside `container: sonic-slave-*`:

```bash
PYTHONPATH=$(Build.SourcesDirectory)/ci python3 -m buildenv_setup \
    --repo-dir $(Build.SourcesDirectory) --scope build \
    --arch <arch> --debian-version <deb> --branch $(BUILD_BRANCH)
./build-env/build.sh
```

`buildenv_setup` installs the apt/pip packages, downloads + installs the upstream
libyang DEBs and sonic-yang wheels, and runs the redis / yang-models post-install
hooks. Then `build.sh` builds the package.

## Local development

```bash
cd build-env
DEBIAN_VERSION=bookworm docker compose run --rm build   # build .debs
DEBIAN_VERSION=bookworm docker compose run --rm shell   # poke around
```

The image bakes the dependency setup; your source is mounted live at
`/workspace`, so ordinary edits don't require an image rebuild (only changes under
`build-env/` do). Parity with CI covers **building and C++ unit tests**; the full
VS/DVS test suite needs CI-like infrastructure (KVM/privileged/nested-docker) and
is not part of local build parity.

> Artifact download uses the Azure DevOps REST API. Public SONiC pipelines are
> readable anonymously; if your environment needs auth, set `AZURE_DEVOPS_EXT_PAT`,
> or pre-stage bundles and pass `--upstream-staged-dir`.
