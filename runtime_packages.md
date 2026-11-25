# Generated runtime package metadata

`//:runtime_packages.json` is generated from this repository's existing
`debian/control` and `debian/changelog`, the package-to-TAR mapping in
`BUILD.bazel`, and the exact source-built runtime payloads. Change package names,
descriptions and explicit dependency policy in the Debian files; there is no
second checked-in copy of those fields.

The shared `runtime_packages` rule computes shared-library dependencies from the
selected payloads and their declared source/APT dependency metadata. Its output
also carries the package owner's public runtime target. The consuming OCI rule
checks that target, hashes the actual TAR and reads the selected module version
for source provenance. Architecture and feature selection follow the Bazel
configuration. Unresolved dependency substitutions fail the build.

The rule generates metadata and library dependency information only. It does
not build Debian packages, run package installation scripts, or change the
legacy Make package recipes.

Build the metadata with `bazel build //:runtime_packages.json`. Native CI builds
this target alongside the corresponding runtime TARs.

## Required package files

The producer declares its required paths in `BUILD.bazel`, next to the
package-to-TAR mapping. The metadata action checks those paths in the built
runtime TAR before emitting metadata. For example, a TAR missing
`usr/lib/{multiarch}/libswsscommon.so.0` fails even if the library compiled.
`{multiarch}` resolves to the selected AMD64 or ARM64 library directory.

Common owns the library names, Python extension, CLI and canonical
`run/redis/sonic-db/database_config.json` path. The runtime TAR supplies Lua
scripts and database configuration as root-owned `0644` data files, so images
can consume them without fixing permissions or rewriting the configuration path.
Its package test checks raw TAR ownership and data modes and compares the
installed database configuration with `common/database_config.json`.
The base image's existing `/var/run -> /run` link preserves access for callers
using `/var/run`; the TAR does not replace that link or change legacy Debian
packaging.

Runtime library symlinks explicitly use mode `0777`, preserving the metadata
expected when the same producer package is installed in a parent OCI layer.

## Shared netlink dependencies

Common links the three native shared targets owned by `libnl3` and declares
`@libnl3//:runtime_packages.json` as source package metadata. Consumers such as
Linkmgrd can link Common and libnl together without embedding another copy of
libnl in Common. Images must include the corresponding libnl runtime TARs;
Common's TAR does not absorb another owner's files.

`//tests/shared_netlink:consumer_test` links the original shared providers and
executes Common's C API plus base, route and netfilter libnl APIs. The existing
ELF test checks Common's base/route dependencies and rejects embedded libnl
symbols. Native CI preserves both architectures and YANG modes, and its
installed check verifies that the loaded Common, CLI and libnl files match the
built TARs without library-path overrides. This does not build DEBs or establish
complete Linkmgrd/container acceptance.
