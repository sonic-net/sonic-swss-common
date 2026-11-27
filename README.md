*static analysis:*

[![Total alerts](https://img.shields.io/lgtm/alerts/g/sonic-net/sonic-swss-common.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/sonic-net/sonic-swss-common/alerts/)
[![Language grade: Python](https://img.shields.io/lgtm/grade/python/g/sonic-net/sonic-swss-common.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/sonic-net/sonic-swss-common/context:python)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/sonic-net/sonic-swss-common.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/sonic-net/sonic-swss-common/context:cpp)

*sonic-swss-common builds:*

[![master build](https://dev.azure.com/mssonic/build/_apis/build/status/Azure.sonic-swss-common?branchName=master&label=master)](https://dev.azure.com/mssonic/build/_build/latest?definitionId=9&branchName=master)
[![202205 build](https://dev.azure.com/mssonic/build/_apis/build/status/Azure.sonic-swss-common?branchName=202205&label=202205)](https://dev.azure.com/mssonic/build/_build/latest?definitionId=9&branchName=202205)
[![202111 build](https://dev.azure.com/mssonic/build/_apis/build/status/Azure.sonic-swss-common?branchName=202111&label=202111)](https://dev.azure.com/mssonic/build/_build/latest?definitionId=9&branchName=202111)
[![202106 build](https://dev.azure.com/mssonic/build/_apis/build/status/Azure.sonic-swss-common?branchName=202106&label=202106)](https://dev.azure.com/mssonic/build/_build/latest?definitionId=9&branchName=202106)
[![202012 build](https://dev.azure.com/mssonic/build/_apis/build/status/Azure.sonic-swss-common?branchName=202012&label=202012)](https://dev.azure.com/mssonic/build/_build/latest?definitionId=9&branchName=202012)
[![201911 build](https://dev.azure.com/mssonic/build/_apis/build/status/Azure.sonic-swss-common?branchName=201911&label=201911)](https://dev.azure.com/mssonic/build/_build/latest?definitionId=9&branchName=201911)

# SONiC - SWitch State Service Common Library - SWSS-COMMON

## Description
The SWitch State Service (SWSS) common library provides libraries for database communications, netlink wrappers, and other functions needed by SWSS.

## Getting Started

### Build from Source

Checkout the source:

    git clone --recursive https://github.com/sonic-net/sonic-swss-common


Install build dependencies:

    sudo apt-get install make libtool m4 autoconf dh-exec debhelper cmake pkg-config \
                         libhiredis-dev libnl-3-dev libnl-genl-3-dev libnl-route-3-dev \
                         libnl-nf-3-dev swig3.0 libpython2.7-dev libpython3-dev \
                         libgtest-dev libgmock-dev libboost-dev

Build and Install Google Test and Mock from DEB source packages:

    cd /usr/src/gtest && sudo cmake . && sudo make

You can compile and install from source using:

    ./autogen.sh
    ./configure
    make && sudo make install

You can also build a debian package using:

    ./autogen.sh
    ./configure
    dpkg-buildpackage -us -uc -b

### Build with Bazel

Clone the fork containing the standalone Bazel build:

```sh
git clone --recursive https://github.com/sonic-net/sonic-swss-common.git
cd sonic-swss-common
```

Use a native AMD64 or ARM64 Linux environment with Debian Trixie userspace.
The standalone Bazel build supports YANG enabled (the default) and disabled.
Install [Bazelisk](https://bazel.build/install/bazelisk) as `bazel`;
`.bazelversion` selects Bazel 8.5.1. Bazel downloads the native compiler
toolchain and library dependencies.

Install the host build tools:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  binutils build-essential ca-certificates git python3 tar
```

From the repository root, build the C++ shared library and package archives.
Rust dependencies come from the pinned `sonic-rust-deps` registry module;
no separate Rust preparation command is needed. Common and SWSS use its shared
Serde targets so their public serialization traits remain compatible.

```sh
# Native AMD64
bazel build --//tools/bazel:yang_modules=True \
  //:libswsscommon_shared \
  //dist:libswsscommon_pkg \
  //dist:libswsscommon_pkg.debug_symbols \
  //dist:sonic-db-cli_pkg \
  //pyext:swsscommon_pkg
```

For native ARM64, add `--config=aarch64` after `build`. The package archives
are written to `bazel-bin/dist/libswsscommon_pkg.tar`,
`bazel-bin/dist/libswsscommon_pkg.debug_symbols.tar`,
`bazel-bin/dist/sonic-db-cli_pkg.tar`, and
`bazel-bin/pyext/swsscommon_pkg.tar.gz`. To build without YANG, pass
`--//tools/bazel:yang_modules=False`.

Enabled deployments also need the prepared models under `/usr/local/yang-models`
and the matching native libyang runtime. These dependencies are built, tested,
and published by the [SONiC Bazel registry](https://github.com/sonic-net/sonic-bazel-registry/actions),
separately from Common's four package archives. Use the module versions selected
by `MODULE.bazel` and retain libyang's matching debug archive. See the
[deployment inputs](docs/bazel.md#dependency-runtime-packages) for details.

The runtime package contains the library and `swssloglevel` with their debug
information removed. The matching detached information is in
`libswsscommon_pkg.debug_symbols.tar`, derived from the same linked files.

To keep source-level debug information embedded in the raw shared library,
build it with explicit compiler and strip settings. These flags work with either
YANG mode:

```sh
bazel build --copt=-g --strip=never \
  //:libswsscommon_consolidated.so
```

Add `--//tools/bazel:yang_modules=False` for disabled mode and
`--config=aarch64` for native ARM64. The symbols are embedded in
`bazel-bin/libswsscommon_consolidated.so/libswsscommon.so.0`. The current
compiler toolchain needs `--copt=-g`; selecting `-c dbg` alone does not add
debug information.

See the [Bazel build guide](docs/bazel.md) for the complete build and test
commands, Go bindings, supported configuration, and GitHub Actions artifacts.

### Build with Google Test
1. Rebuild with Google Test
```
$ ./autogen.sh
$ ./configure --enable-debug 'CXXFLAGS=-O0 -g'
$ make clean
$ GCC_COLORS=1 make
```

2. Start redis server if not yet:
```
sudo sed -i 's/notify-keyspace-events ""/notify-keyspace-events AKE/' /etc/redis/redis.conf
sudo service redis-server start
```

3. Run unit test:
```
tests/tests
```

## Need Help?

For general questions, setup help, or troubleshooting:
- [sonicproject on Google Groups](https://groups.google.com/g/sonicproject)

For bug reports or feature requests, please open an Issue.

## Contribution guide

Please read the [contributors guide](https://github.com/sonic-net/SONiC/blob/gh-pages/CONTRIBUTING.md) for information about how to contribute.

All contributors must sign an [Individual Contributor License Agreement (ICLA)](https://docs.linuxfoundation.org/lfx/easycla/v2-current/contributors/individual-contributor) before contributions can be accepted. This process is managed by the [Linux Foundation - EasyCLA](https://easycla.lfx.linuxfoundation.org/) and automated
via a GitHub bot. If the contributor has not yet signed a CLA, the bot will create a comment on the pull request containing a link to electronically sign the CLA.

### GitHub Workflow

We're following basic GitHub Flow. If you have no idea what we're talking about, check out [GitHub's official guide](https://guides.github.com/introduction/flow/). Note that merge is only performed by the repository maintainer.

Guide for performing commits:

* Isolate each commit to one component/bugfix/issue/feature
* Use a standard commit message format:

>     [component/folder touched]: Description intent of your changes
>
>     [List of changes]
>
> 	  Signed-off-by: Your Name your@email.com

For example:

>     swss-common: Stabilize the ConsumerTable
>
>     * Fixing autoreconf
>     * Fixing unit-tests by adding checkers and initialize the DB before start
>     * Adding the ability to select from multiple channels
>     * Health-Monitor - The idea of the patch is that if something went wrong with the notification channel,
>       we will have the option to know about it (Query the LLEN table length).
>
>       Signed-off-by: user@dev.null


* Each developer should fork this repository and [add the team as a Contributor](https://help.github.com/articles/adding-collaborators-to-a-personal-repository)
* Push your changes to your private fork and do "pull-request" to this repository
* Use a pull request to do code review
* Use issues to keep track of what is going on

### Common Go bindings for gNMI

Bazel gNMI consumers depend on `@sonic_swss_common//goext:swsscommon`.
The owner generates and compiles the SWIG binding once. Both
`github.com/sonic-net/sonic-swss-common/goext` and gNMI's existing
`github.com/sonic-net/sonic-gnmi/swsscommon` import refer to that same Go archive.
Keep the gNMI source imports and native Make generator unchanged; resolve the
legacy import to this owner target in the consumer's BUILD files.

`//goext/tests/gnmi:alias_test` checks that both spellings expose identical Go
types and interoperate through the owner-built C++ library. Native AMD64 CI runs
it with YANG enabled and disabled. A consumer root still selects its Go SDK and
must validate its own gNMI API and service behavior.
## Explicit DBConnector authentication

Existing endpoint constructors keep their transport selection and do not consult writer
policy. To request native local authentication, pass the namespace (or
`SonicDBKey`) followed by `authenticate=true`:

```cpp
swss::DBConnector db("STATE_DB", 1000, false, std::string(), true);
```

The existing Python binding exposes the same overload:

```python
db = swsscommon.DBConnector("STATE_DB", 1000, False, "", True)
```

Authentication runs before database selection and is repeated for copied and
secondary connections. It supports explicitly selected Unix sockets. Startup
provides the root-owned `/run/redis-auth/local.json` endpoint policy and
`local-system/writer.secret`. Missing policy or an unmatched endpoint leaves the
default account in use. Each policy entry also requires a numeric `server_uid`.
Before reading the writer credential, the connector checks the connected Unix
peer with `SO_PEERCRED` and requires that UID to match the startup-owned policy.
The image must reserve that UID exclusively for the Redis service. A non-root
caller unable to read the secret remains a reader; invalid policy or rejected
credentials fail construction. Opt-in does
not grant access to credentials. Nonzero constructor timeouts also bound AUTH.

This addition changes C++ object layout; rebuild dependent native applications
and language bindings with the matching library. Raw-client adapters are separate follow-ups.

The focused tests require an explicitly selected disposable root container:
`SWSS_AUTH_TEST_CONTAINER=1 pytest --noconftest tests/test_redis_authentication.py`.
They refuse to replace an existing authentication-policy directory.


## Explicit higher-level connector authentication

`SonicV2Connector(use_unix_socket_path=True, authenticate=True)` requests native
authentication for named connections and recovery. Both options default to false;
authentication does not change the requested transport. `ConfigDBConnector` and
`ConfigDBPipeConnector` accept the same options.

Native DBInterface callers select the transport with `set_redis_kwargs` and call
`set_authentication(key)` before the first `connect` to opt in.
Authentication and endpoint selection are retained through subscription recovery.

## Explicit authentication through C and Rust

The C interface provides `SWSSDBConnector_new_named_with_auth`,
`SWSSDBConnector_new_keyed_with_auth`, `SWSSSonicV2Connector_new_with_auth` and
`SWSSConfigDBConnector_new_with_auth`. Rust exposes corresponding `new_with_auth`
constructors. Existing entry points keep native authentication disabled.
