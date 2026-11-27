# Standalone Bazel build

The standalone build uses Bazel 8.5.1, selected by `.bazelversion`. Install
[Bazelisk](https://bazel.build/install/bazelisk) and invoke it as `bazel`.
Dependencies come from the configured SONiC Bazel registry and the Bazel Central
Registry; the native GCC 14.2 toolchain and Debian package inputs are downloaded
by Bazel. Local builds and CI use the maintained SONiC registry `main` branch,
followed by the Bazel Central Registry.

Local builds and normal CI use `sonic-bazel-registry/main` in `.bazelrc`, with
Bazel Central Registry as the separate third-party registry. Module versions
and source checksums still select the dependencies; the registry URL does not
select their latest source code.

Package artifacts retain `effective.bazelrc` and the generated module lock for
dependency inspection.

## Validate the shared-library update

This PR uses the small `sonic-build-infra` adapter needed to consume native
`cc_shared_library` outputs from C++, Go, and Rust targets
([infrastructure PR #51](https://github.com/securely1g/sonic-build-infra/pull/51)).
The native tests use the linker's default RUNPATH behavior. Their disposable
Trixie container installs the required Debian runtime packages in normal system
locations. For example, ZeroMQ can find `libsodium.so.23` under `/usr/lib` without
rewriting ZeroMQ's ELF file or making Common's search path apply recursively.

The adapter is published on the maintained registry through
[registry PR #81](https://github.com/securely1g/sonic-bazel-registry/pull/81).
Local builds, native CI and CodeQL use the canonical registry `main` endpoint
from `.bazelrc`. The declared module version selects the landed infrastructure
source; CI records the resolved registry revision and generated module lock.

```sh
bazel build //:libswsscommon_shared
```

Add `--config=aarch64` on native ARM64.

## Environment

Use a native AMD64 or ARM64 Linux environment with Debian Trixie userspace. The
toolchains require the execution CPU and target CPU to match. AMD64 is the
default; native ARM64 requires `--config=aarch64`. The target libraries link
against Trixie's glibc 2.41, so CI runs inside a Trixie container on each native
GitHub-hosted runner.

CI uses `debian:trixie-20260918` and installs these host tools. The package tests
use `readelf`, `objcopy`, and `nm` from `binutils`, plus GDB, Python, and `tar`.

```sh
apt-get update
apt-get install -y --no-install-recommends \
  binutils build-essential ca-certificates gdb git python3 tar
```

### Linux sandbox in CI

Normal native and cgo CI use `--config=ci-linux-sandbox` on AMD64 and ARM64.
This requires Bazel's `linux-sandbox` for build actions and tests; unavailable
Linux sandbox support fails the job instead of falling back to process-wrapper
or local execution. CodeQL keeps local execution so it can trace compilation.

The disposable GitHub-hosted job containers use these Docker options:

```text
--cap-add=SYS_ADMIN --security-opt=apparmor=unconfined --security-opt=systempaths=unconfined
```

These relax the outer container restrictions so Bazel can create namespaces,
mount a read-only filesystem, and mount `/proc` for its PID namespace. Docker's
default seccomp profile remains enabled. Each job runs an uncached probe that
checks separate user, mount and PID namespaces and denies writes to a file
outside the sandbox. The same probe must fail under process-wrapper execution.
The validation artifacts retain `linux-sandbox.json` with both results and the
actual execution runners. The initial CI build uses a new disk-cache key.

Sandboxing does not pin or hide host software, and the default action network
policy remains unchanged. APT setup, `bazel run`'s final program and the installed
library check run outside Bazel's action sandbox.

### Prepare the native test runtime

Run runtime setup **inside a disposable Trixie container**, after dependency
analysis and before executing native tests. It requires root in that container.
Compilation and linking still use Bazel's declared packages and sysroot.

```sh
bazel build --nobuild --//tools/bazel:yang_modules=False //:libswsscommon_shared
bazel run //tools/bazel/runtime:prepare -- \
  --image-reference debian:trixie-20260918 \
  --output /tmp/common-test-runtime.json
export SONIC_TEST_RUNTIME_ID="$(python3 -c \
  'import json; print(json.load(open("/tmp/common-test-runtime.json"))["SONIC_TEST_RUNTIME_ID"])')"
```

Add `--config=aarch64` for native ARM64. Use `yang_modules=True` when preparing a YANG
build. Analysis resolves the configured targets without compiling them.

The setup helper reads versions, snapshot URLs, checksums, and dependency lists
from the selected build-infra module’s reviewed `apt/apt.lock.json`. APT downloads and installs missing runtime packages; the helper
checks their hashes before installation. It uses only the resolved snapshot
sources and installs no development packages. Existing base packages remain
unchanged. For example, a newer compatible base libc or UUID library stays in
place. This is **tests against the prepared container**: actual installed base versions may
differ from the compilation pins.

The JSON receipt records the build pins, actual installed package versions, and
shared-library hashes. `SONIC_TEST_RUNTIME_ID` fingerprints that receipt and is
passed to Bazel as a test environment input. CI also uses
`--nocache_test_results`. Common and CLI libraries must be absent from system
paths during these tests, so they cannot mask missing Bazel outputs.

## Rust dependency resolution

Common uses the third-party Rust targets published by the pinned
`sonic-rust-deps` registry module. That module resolves its committed Cargo
inputs with `rules_rs` and owns the shared Serde features. Common's Rust BUILD
file lists its five normal dependencies explicitly; it does not create a second
Cargo dependency graph. Their versions and checksums match Common's `Cargo.lock`.

A clean checkout needs no preparation script or `Cargo.Bazel.lock`. Keep
Common's Cargo files in Git for Cargo builds. When changing dependencies, update
the owning shared module and check its lock against the affected Cargo inputs
before updating the module pin. The generated `MODULE.bazel.lock` stays ignored
and is retained with CI evidence.

The Bazel 8.5.1 root selects `rules_cc` 0.2.20 with a
`single_version_override`. Distroless uses its private C++ import rule, and
rules_cc 0.2.21/0.2.22 require a runtime toolchain type absent from Bazel 8.5.1.
An external root must repeat this override because module overrides do not
propagate from dependencies.

## Go build inputs

This branch selects `rules_go 0.65.0.sonic.1`, which carries the relative
cgo header-path repair on upstream 0.65.0. Standalone builds use Go SDK 1.25.0. The original
upstream regression cases pass with the repair on native AMD64 and ARM64 using
Bazel 8.5.1 ([validation run](https://github.com/securely1g/sonic-swss-common/actions/runs/37182396550)).

The version is available on the maintained registry through
[registry PR #37](https://github.com/securely1g/sonic-bazel-registry/pull/37).
Local builds, native CI and CodeQL use the canonical `main` registry endpoint,
with BCR as the other registry. The evidence helper records the observed registry
revision and effective configuration without changing `.bazelrc`.

To retain the same evidence locally, use an empty evidence directory:

```sh
python3 tools/bazel/go_validation.py record /tmp/common-go-validation
# Run the appropriate native build/test commands below.
python3 tools/bazel/go_validation.py collect /tmp/common-go-validation
```

Add `--config=aarch64` to `collect` on native ARM64. Collection checks the
resolved module version, reviewed cgo source hash, Bazel 8.5.1, and Go 1.25.0,
and retains the generated module lock, source files, SDK receipts and repository
definition. It also verifies that the tracked checkout remains unchanged.

The existing six native jobs retain YANG enabled/disabled coverage and the
hiredis cgo probe with `external_include_paths` enabled and disabled. Common's
SWIG wrapper still compiles its generated C++ separately; the dedicated upstream
regression above covers the relative `cc_import` path that this arrangement does
not exercise. Package outputs remain tar archives.

## Build and test

Run the following from the repository root in Bash. Define the common and YANG
target lists, then run the command matching the machine's native CPU and desired
feature mode, after preparing the container runtime above. CI runs both modes
with these explicit labels. Explicit labels
make a required output's platform incompatibility fail the invocation instead
of being skipped by a wildcard.

```bash
targets=(
  //:libswsscommon
  //:libswsscommon_shared
  //:libswsscommon_consolidated.so
  //:swssloglevel
  //crates/swss-common:bindings_dir
  //crates/swss-common:swss_common
  //crates/swss-common:swss_common_test
  //tools/bazel/rust:shared_serde_test
  //dist:libswsscommon_pkg
  //dist:libswsscommon_pkg.debug_symbols
  //dist:libswsscommon_sdk
  //dist:sonic-db-cli_pkg
  //dist:sonic-db-cli_pkg.debug_symbols
  //pyext:swsscommon_pkg
  //pyext:swsscommon_pkg.debug_symbols
  //goext:swsscommon
  //goext:swsscommon_runtime_test
  //tests:status_code_util_test
  //tests:saiaclschema_ut
  //tests:notification_queue_ut
  //tests:interface_ut
  //tests:vrf_ut
  //tests:shared_library_runtime_test
  //tests:shared_library_elf_test
  //sonic-db-cli:shared_library_test
  //dist:libswsscommon_package_test
  //dist:libswsscommon_sdk_test
  //dist:package_timestamps_test
  //pyext:swsscommon_package_test
)

yang_targets=(
  //common:cfg_schema_generated
  //tests:defaultvalueprovider_ut
)

# Native AMD64, YANG enabled (the default)
bazel test --test_env=SONIC_TEST_RUNTIME_ID --nocache_test_results --//tools/bazel:yang_modules=True --test_output=errors "${targets[@]}" "${yang_targets[@]}"

# Native AMD64, YANG disabled
bazel test --test_env=SONIC_TEST_RUNTIME_ID --nocache_test_results --//tools/bazel:yang_modules=False --test_output=errors "${targets[@]}"

# Native ARM64, YANG enabled
bazel test --config=aarch64 --test_env=SONIC_TEST_RUNTIME_ID --nocache_test_results --//tools/bazel:yang_modules=True --test_output=errors "${targets[@]}" "${yang_targets[@]}"

# Native ARM64, YANG disabled
bazel test --config=aarch64 --test_env=SONIC_TEST_RUNTIME_ID --nocache_test_results --//tools/bazel:yang_modules=False --test_output=errors "${targets[@]}"
```

`bazel test` builds the listed libraries, binaries, bindings, and packages as
well as running the listed tests. CI also checks Bazel file formatting:

```sh
# Native AMD64
bazel run //tools/bazel/buildifier:buildifier.format.check

# Native ARM64
bazel run --config=aarch64 //tools/bazel/buildifier:buildifier.format.check
```

The formatting target leaves lint disabled while legacy compatibility helpers
remain in the build. The existing `buildifier.check` target remains
available for reviewing those lint warnings.

## Shared libraries and runtime paths

`//:libswsscommon_consolidated.so` is a `cc_shared_library` target in the root
BUILD file. It links directly to `libswsscommon.so.0` in the Bazel output
directory. The historical target label remains available to consumers;
it no longer names a directory containing a copied library. The rule includes
all object files from its direct `//:common` dependency, so the complete public
C API is exported. Ordinary static consumers still select only the object files
they need, which allows unit tests to supply mocks.

`//sonic-db-cli:libsonicdbcli.so.0` is defined beside its source in
`sonic-db-cli/BUILD.bazel`, producing `sonic-db-cli/libsonicdbcli.so.0`.
It declares Common as a `dynamic_deps` dependency: it calls the shared Common
library rather than linking another copy into the CLI library. Go and Rust
consumers use a small `shared_library_cc_info` adapter from `sonic-build-infra`.
It forwards the native rule's linker information and runtime files in the
`CcInfo` format those language rules expect; it does not copy or reimport either
library.

Common's source build uses Bazel 8.5.1. P4RT can remain on Bazel 6.4.0 by
importing the built library and headers from the SDK below. It does not load
Common's BUILD files, so the shared-library rules no longer need separate
`shared/` packages.

Imported Debian libraries use the prepared container's system libraries for
dependencies that do not have their own Bazel search path. For example, ZeroMQ
can find its indirect `libsodium` dependency through the normal system loader.
The tests use default linker flags and require RUNPATH rather than RPATH.
Running them on an unprepared host is not the supported native test environment.

`//tests:shared_library_elf_test` checks both raw libraries' runtime names,
RUNPATH tags, exported Common C APIs, Boost dependency and the CLI's shared
dependency on Common. The existing C, C++ and Rust tests exercise the uninstalled
library paths. The Go check loads Common with its declared Trixie runtime. Run
these tests on native AMD64 and ARM64 with YANG enabled and disabled when
changing the shared-library rules or toolchain runtime paths.

Packaging is a separate step. Runtime archives retain Make's
`/usr/lib/<architecture>/` layout and remove paths into Bazel's private library
directories. Installed containers resolve those libraries through their normal
system loader configuration; they do not use the uninstalled build layout.

After all raw native tests pass, CI installs the current Common and CLI runtime
tars in the same disposable container. It copies the existing dynamic C and C++
smoke consumers outside the Bazel output/runfiles tree and runs them with loader
environment overrides removed. A loader check requires both installed libraries
to come from `/usr/lib/<architecture>/` and match the archive hashes; the receipt
also records their build IDs. This tests the shared CLI-to-Common dependency,
which running the statically linked `sonic-db-cli --help` alone would not cover.

For the no-YANG mode, reproduce that final check with:

```sh
python3 -B tools/bazel/runtime/installed_check.py \
  --common-package bazel-bin/dist/libswsscommon_pkg.tar \
  --cli-package bazel-bin/dist/sonic-db-cli_pkg.tar \
  --common-probe bazel-bin/tests/shared_library_runtime_test \
  --cli-probe bazel-bin/sonic-db-cli/shared_library_test \
  --output /tmp/common-installed-runtime.json
```

For YANG, also pass `--libyang-package` with the path returned by
`bazel cquery --output=files @libyang//:libyang_pkg` (add `--config=aarch64` on
ARM64). The Go/Python tests already build this source-owned runtime tar. Start a
fresh container before another raw native test run, because Common is now
installed in the previous container's system paths.

## Library and headers for P4RT

`//dist:libswsscommon_sdk` packages Common's built library and public headers.
P4RT can import these files with Bazel 6.4.0 while Common builds them with
Bazel 8.5.1. P4RT does not compile Common's sources or evaluate Common's BUILD
files in this path.

Build and check the YANG-enabled SDK on native AMD64:

```sh
bazel test --//tools/bazel:yang_modules=True //dist:libswsscommon_sdk_test
```

Add `--config=aarch64` on native ARM64. Use
`--//tools/bazel:yang_modules=False` for a no-YANG SDK. The library and generated
`cfg_schema.h` use the same selected mode; the production P4RT handoff uses
YANG enabled, matching the usual Common build in Make.

The output is `bazel-bin/dist/libswsscommon_sdk.tar`:

```text
include/swss/                 All public Common headers
include/swss/cfg_schema.h     Schema generated for the selected YANG mode
include/swss/c-api/           Public C headers
lib/libswsscommon.so.0        Real shared library
lib/libswsscommon.so          Relative link to libswsscommon.so.0
```

The SDK test checks every packaged header against its build input, the library's
architecture and runtime name, its YANG dependency, and equality with the ELF
in `libswsscommon_pkg.tar`. The normal deployment rule removes Bazel's private
library search paths before packaging. Matching debug symbols remain in
`libswsscommon_pkg.debug_symbols.tar`.

The consumer imports `lib/libswsscommon.so.0` with `cc_import` and exposes
`include` and `include/swss` as header roots. External development and runtime
dependencies are still required: hiredis, libnl3, ZeroMQ, Boost, UUID,
nlohmann-json headers, and libyang when enabled. Use the matching Trixie build
environment and the SDK for the target architecture. The SDK is not a complete
sysroot and does not install those dependencies or change P4RT's Bazel version.

## Rust library

`//crates/swss-common:swss_common` is the public Rust library. Common owns its
Rust sources, generated C API bindings, and the
link to `//:libswsscommon_shared`. The default library has Cargo's `async`
feature disabled. Its four existing unit tests run without a Redis server:

```sh
bazel test //crates/swss-common:swss_common_test \
  //tools/bazel/rust:shared_serde_test
```

Add `--config=aarch64` on native ARM64. CI runs the library and unit tests on
native AMD64 and ARM64 in both YANG modes. Redis-backed Cargo integration tests remain outside these standalone library tests.

Consumers that enable Cargo's `async` feature, such as DASH HA's `swbus-actor`
and `swss-common-bridge`, use `//crates/swss-common:swss_common_async`. Common
builds this variant from the same sources and bindings, with the shared Tokio
dependency. It retains the default target for consumers without that feature.

The existing test-helper crate has two public targets:

- `//crates/swss-common-testing:swss_common_testing` uses the default Common library.
- `//crates/swss-common-testing:swss_common_testing_async` uses the async Common library.

Select the matching pair: Rust types from separately compiled Common variants
are distinct. Consumers must not compile another copy of Common or its testing
crate. Both variants reuse `sonic-rust-deps` for their third-party dependencies.

`//tools/bazel/rust:testing_interface_test` and
`//tools/bazel/rust:async_testing_interface_test` verify the shared public types
and Serde traits. The async test also executes Common's blocking-task adapter
with the shared Tokio runtime, using an invalid socket string so it needs no
host Redis server. `//crates/swss-common:swss_common_async_test` runs the existing
Common unit tests with async enabled. CI runs and retains all these results in
both existing YANG modes.

Bazel consumers depend on `@sonic-swss-common//crates/swss-common:swss_common`.
Common and SWSS both use `@sonic_rust_deps//:serde` and
`@sonic_rust_deps//:serde_core`, currently version 1.0.228. The shared module owns
the compiled traits and their feature selection; Common no longer exports Serde
aliases. Direct dependencies work when Common is built alone or imported by
another root, without repository overrides. The `shared_serde_test` exercises
Common's `CxxString` through both shared trait APIs, and SWSS's downstream
`common_rust_test` checks the boundary with a JSON roundtrip.

Common retains its own library, native link dependencies, bindings, and tests.
Consumers must register compatible Rust and bindgen toolchains because Common's
standalone toolchain registrations are development dependencies. When changing
third-party dependencies, update the tracked Cargo inputs and validate the
downstream consumer as well as Common's tests.

The underlying `//crates/swss-common:bindings_dir` target remains public. It
generates `bindings.rs` from every header in `common/c-api` and places it in the
directory layout expected by `OUT_DIR`. The header inventory comes from the
same Bazel filegroup used by the native Common library. The target uses
`--with-derive-partialeq`, matching `build.rs`.

Standalone builds select Rust 1.90.0 and use the Rust and bindgen integration
provided by `rules_rs` 0.1.0. Cargo's unchanged `build.rs` uses bindgen 0.70.1.
Tests validate compilation and behavior; they do not claim byte-for-byte
equality with Cargo's generated bindings. The Rust library is a source
dependency; it is not distributed as a precompiled Rust package.

## Build artifacts

Each successful Bazel job uploads its package archives. Open the repository's
**Actions** tab, select a successful **Bazel** workflow run, and download the
artifact for your architecture and feature mode from the **Artifacts** section:

- `sonic-swss-common-yang-AMD64`
- `sonic-swss-common-yang-ARM64`
- `sonic-swss-common-no-yang-AMD64`
- `sonic-swss-common-no-yang-ARM64`

YANG-enabled and YANG-disabled validation run in separate native jobs for each
architecture. Each job retains `--sandbox_debug` and its disk-usage snapshots;
its sandboxes do not accumulate with the other feature mode. The `Bazel (AMD64)`
and `Bazel (ARM64)` checks each require both feature jobs for that architecture
to succeed.

The `sonic-swss-common-rust-<architecture>-<mode>` artifacts retain the Rust
unit-test and shared Serde test XML and logs, the Bazel module lockfile for that
mode, Common's source `Cargo.lock`, and the tested source commit and tree.
`<architecture>` is `AMD64` or `ARM64`, and `<mode>` is `enabled` or `disabled`.
The corresponding `sonic-swss-common-go-validation-<architecture>-<mode>`
artifacts retain native Go and registry evidence. Each includes a
`feature-mode.txt` receipt. Package artifact names above remain unchanged.
The CodeQL job retains the Cargo and Bazel module locks.

Every package download contains these seven archives:

- `libswsscommon_pkg.tar`: `libswsscommon.so.0`, `libsonicdbcli.so.0`, `swssloglevel`, Lua files, and
  database configuration.
- `libswsscommon_pkg.debug_symbols.tar`: detached debug information for the
  two runtime libraries and `swssloglevel` in `libswsscommon_pkg.tar`.
- `libswsscommon_sdk.tar`: the same Common runtime library and public headers
  for consumers that build with a different Bazel version, including P4RT.
- `sonic-db-cli_pkg.tar`: database CLI.
- `sonic-db-cli_pkg.debug_symbols.tar`: detached symbols for the database CLI.
- `swsscommon_pkg.tar.gz`: Python bindings with a stripped native extension.
- `swsscommon_pkg.debug_symbols.tar`: matching debug information for that extension.

### Dependency runtime packages

Common's artifacts contain its own library, bindings, CLI, and symbols. Enabled
deployments also require packages owned by the
[SONiC Bazel registry](https://github.com/sonic-net/sonic-bazel-registry/actions):

- `sonic-yang-models`: `//:yang_models_pkg` installs the prepared model payload
  under `/usr/local/yang-models`, the path used by `DefaultValueProvider`.
- `libyang`: `//:libyang_pkg` supplies the native runtime, and
  `//:libyang_pkg.debug_symbols` supplies its matching detached symbols.

Select a successful **Registry CI** workflow for the module versions pinned
by this build and download `registry-ci-<module>-<version>-<architecture>`
(`amd64` or `arm64`). Its `outputs.json` maps declared build targets to retained
files under `outputs/`, including their hashes. Keep each runtime/debug pair
from the same run. Registry module READMEs linked in the
[YANG input guide](../tools/bazel/yang/README.md) document the package targets.
Common CI does not copy or upload these dependency archives. Its staged Python
and Go tests still consume the pinned libyang runtime to verify integration.
Install the model and libyang runtime with Common's runtime archive and the
remaining Trixie runtime libraries. The Python bindings archive is required by
Python consumers. Production Make supplies the models through the
`sonic_yang_models` wheel; the registry tar carries the same model payload.

The standalone build produces tar archives. Debian dependency metadata, the
`libswsscommon-dev` package, and wheel packaging remain owned by the Make
workflow. GitHub requires you to sign in to download workflow artifacts.

### Reproducible package timestamps

The registry's `tar.bzl` patch adds an optional `default_mtime` setting to `tar()`.
It supplies timestamps only where the manifest has no explicit or inherited
time. `sonic_deploy_tar` selects `1672560000` for the library's runtime archive;
the CLI calls `tar(default_mtime = "1672560000")` directly. The library symlinks
retain their explicit `time=0`. The Lua package already uses an automatically
generated manifest with deterministic timestamps.

Common's package test checks its actual library, program, configuration and
symlink headers. Generic manifest and reproducibility tests accompany the
`tar.bzl` patch in the registry, and deployment integration tests remain in
`sonic-build-infra`. The root module selects `tar.bzl@0.10.5.sonic.2`.

## Debug symbols

Build the runtime package and its matching detached symbols together:

```sh
bazel build --//tools/bazel:yang_modules=True \
  //dist:libswsscommon_pkg //dist:libswsscommon_pkg.debug_symbols \
  //dist:sonic-db-cli_pkg //dist:sonic-db-cli_pkg.debug_symbols
```

Use `--//tools/bazel:yang_modules=False` for disabled mode, and add
`--config=aarch64` on native ARM64. Packaging uses `sonic_deploy_tar` with
`force_debug_build = True`, which applies `--copt=-g`, `--strip=never`, and a
linker build ID to the package inputs while retaining the selected compilation
mode and optimization settings. It derives the runtime copy and detached debug
file from the same linked ELF. The runtime package has its DWARF sections removed;
the separate archive stores them under `/usr/lib/debug/.build-id/`.

The package test validates the library filename, SONAME, symlinks, build IDs,
debug-link checksums, and GDB source-line lookup. It checks both shared libraries,
`swssloglevel` and `sonic-db-cli` for detached debug information. A separate C++
consumer links to the public CLI library and runs its help path without Redis.
The Python package archive has its own packaging path.

The direct targets `//:libswsscommon`, `//:libswsscommon_shared`, and
`//:libswsscommon_consolidated.so` continue to use the caller's compilation
settings. To build the raw shared library with embedded debug information, use
`--copt=-g --strip=never`. The package targets apply their debug settings through
the deployment transition, so those flags are unnecessary for the package command
above.

## YANG configuration

`--//tools/bazel:yang_modules=True` is the default. It generates `cfg_schema.h`
from the production model set, compiles `DefaultValueProvider` and both decorator
table classes with libyang, and exposes those classes and generated table-name
constants through the Python bindings. The disabled setting selects the minimal
schema stub and omits the YANG native sources, libyang dependency, and Python
API entries.

Registry module `sonic-yang-models` prepares the production model set with its
manifest, raw models, templates, and locked Python tools. Common's unchanged
`gen_cfg_schema.py` consumes that prepared directory through the registry's
`sonic-yang-mgmt` Python library. Schema generation runs on the execution
platform, including its Python 3.13 and libyang inputs; the library's native
dependencies follow the target platform. The generator declares its inputs and
requests network blocking, with package installer network access disabled.

The source revisions, libyang-Python patch provenance, and the difference from
the standalone Azure build inputs are recorded in the
[YANG input guide](../tools/bazel/yang/README.md). The public
`//tools/bazel:cfg_schema` label setting remains available for downstream Bazel
6 builds that supply a Make-generated header. Standalone Bazel 8 uses a selected
default header for the enabled and disabled modes.

Common CI checks the integration with these dependencies. The enabled C++
fixture loads models through the shared library; package tests check native
feature symbols and import and construct the enabled Python class from the
archive. Registry CI independently checks the model package, management
library, libyang runtime/package, and Python binding. PCRE2 and xxHash remain
static implementation dependencies of native libyang.

The Python package and Go consumer tests stage the same source-built libyang
and pinned Trixie runtime packages used by the build. The Go test exercises
wrapped value types and calls `Select` in the current shared library without
Redis. It checks that the current library is loaded and that hiredis and the
enabled mode's libyang come from the staged runtime; disabled mode must not load
libyang.

The Go binding target is included in CI. Its Redis-backed integration test,
`//goext:swsscommon_test`, is tagged `manual` and requires the expected Redis
endpoints and database configuration, matching Trixie native libraries, and a
loader configuration that can find them. Run that test separately in an
environment providing those dependencies and services.

## Remote build cache in CI

Native AMD64/ARM64 CI and the cgo-header jobs can use the HTTPS cache at
`https://lgh-bazel-87263989.eastus.cloudapp.azure.com/`. Build actions still run
on the GitHub runner when a matching result is unavailable. The existing
repository and GitHub disk caches remain enabled, and `--nocache_test_results`
keeps the declared tests executing on each run.

Configure the repository Actions secret `BAZEL_REMOTE_AUTH` with the complete
HTTP authorization value: `Basic ` followed by the base64 encoding of
`username:password`. The top-level workflow passes this secret explicitly to
the reusable native workflow. The credential helper reads it from the Bazel
client environment and serves only this cache's HTTPS endpoint. Credentials
must not be placed in `.bazelrc`, URLs or command arguments: CI retains Bazel
configuration and resolution evidence as artifacts.

The setup step verifies authenticated access before enabling the cache. PR
builds read cached results; pushes to `master` and manually dispatched builds
also upload new results. Fork and Dependabot runs without the secret retain
the ordinary build path. These upload flags control client behavior; access
permissions are also governed by the server credential.

The **Bazel remote cache validation** workflow runs only when manually dispatched
from its **Run workflow** button in GitHub Actions. It checks both native
architectures and deliberately uploads validation outputs: it builds Common's
shared library in a fresh output base, then reads it from the remote cache using a
second fresh output base with uploads and the disk cache disabled. The receipt
requires remote hits for Common compilation and linking and matching output
hashes. Only sanitized receipts are uploaded; raw execution logs stay outside
the artifacts directory. This distinguishes remote reuse from local disk reuse.
Normal native CI continues to use the remote cache automatically when credentials
are available; it does not run this separate cache-reuse proof on every PR.

## CodeQL C++ build

The C++ CodeQL job builds with Bazel inside a native AMD64 Debian Trixie
container. CodeQL records compilation and linkage by observing build processes,
so this job creates a fresh Bazel output base after CodeQL initialization, uses
local execution, and disables action caches. Bazelisk and repository download
caches remain available for tools and dependencies. See
[CodeQL and Bazel cache reuse](codeql-cache.md) for the measured cache boundary.

The build passes `--cxxopt=-nostdinc` so local C++ compilation uses the
toolchain's explicit GCC and Debian include paths while retaining Bazel's header
dependency checks.

The build covers the library, command-line tools, and generated Python SWIG
wrapper with YANG enabled. The manual `//tests:codeql_test_sources` target also
compiles all 49 legacy C++ test files, including the YANG fixture source, without
requiring Redis services.

## Python binding debug symbols

`//pyext:swsscommon_pkg` installs the Python wrapper and a stripped native
extension. `//pyext:swsscommon_pkg.debug_symbols` contains that extension's
matching DWARF files under `/usr/lib/debug/.build-id`. Use the runtime archive
in the normal container and add its symbol archive to the debug container.
Both come from the same compilation with `-g` and `--strip=never` before
packaging splits them. The installed-package test imports the selected YANG
API and verifies build IDs, debug-link checksums and GDB source lookup.

The installed archive uses ordinary Python imports; it does not contain the
Bazel runfiles loader. Bazel Python consumers still use `//pyext:swsscommon`.
