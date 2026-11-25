# ZMQ native link interface

Common uses `@trixie//libzmq3-dev:libzmq_cc` to compile against ZMQ's headers
and link its selected shared library. This is the same input already used by
[buildimage's Common patch](https://github.com/securely1g/sonic-buildimage/blob/681be7c5da0302cf9a4b7daf9b4fa1c1c74e9854/tools/bazel/0001-sonic-swss-common-zmq.patch).
Keeping the selection in Common lets gNMI and other consumers use the owner
without repeating that patch.

The older `:libzmq3` aggregate also imports every development dependency's
libraries. For example, it brings libbsd's GNU linker script into Common's
link, then into `//sonic-db-cli:libsonicdbcli.so.0` through `dynamic_deps`.
Bazel 8.5.1 forwards those precompiled libraries without their script-remapping
flags. On an Ubuntu host, linking then tries the missing target-distro file
`/usr/lib/x86_64-linux-gnu/libbsd.so.0.12.2`.

The native per-library target selects the same ZMQ payload and SONAME. It
retains the public headers and complete declared runtime dependency closure;
it avoids treating every transitive development library as another direct
link dependency. No GNU script is rewritten, no SONAME changes, and the static
Common and CLI targets retain their existing source and feature selection.

The AMD64 Ubuntu CI job deliberately checks that the target-distro libbsd file
is absent. It builds the Common/CLI runtime and debug TARs, checks their existing
package contracts, and runs the public CLI ABI probe with the declared Trixie
loader and libraries. Existing native AMD64/ARM64, YANG on/off, binding and
CodeQL jobs remain enabled.
