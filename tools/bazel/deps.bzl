"""The distro dependencies of libswsscommon, in both dependency models.

Under bzlmod these are resolved by rules_distroless into the repository `@trixie`.

Under WORKSPACE we can't do that (rules_distroless is bzlmod-only),
so we fall back to the existing legacy system:
We assume we're running in the slave, and we pick the dependencies via linkopts and copts.

This file abstracts over the two, bundling it into a public API that doesn't care whether we're using bzlmod.

TODO(bazel-ready): Remove the WORKSPACE branch when we only need to support Bazel 8+.
"""

load(":bzlmod.bzl", "IS_BZLMOD")

# Hermetic apt packages, via rules_distroless. bzlmod only.
_APT_DEPS = [
    "@trixie//libhiredis-dev:libhiredis",
    # nlohmann JSON is header-only, including for consumers of the shared library.
    "@trixie//nlohmann-json3-dev:nlohmann-json3_headers",
    "@libnl3//:libnl_3",
    "@libnl3//:libnl_route_3",
    "@libnl3//:libnl_nf_3",
    "@trixie//libzmq3-dev:libzmq_cc",
    "@trixie//uuid-dev:uuid",
    "@trixie//libboost-dev:libboost",
    "@trixie//libboost-serialization-dev:libboost-serialization",
    "//tools/bazel/boost:serialization",
]

_YANG_DEPS = select({
    "//tools/bazel:yang_modules_enabled": ["@libyang//:libyang"],
    "//conditions:default": [],
})

# Legacy ambient dependencies, for WORKSPACE.

_SYSTEM_COPTS = [
    "-I/usr/include/libnl3",  # Expected location in the SONiC slave
]

_SYSTEM_LINKOPTS = [
    "-lpthread",
    "-lhiredis",
    "-lnl-genl-3",
    "-lnl-nf-3",
    "-lnl-route-3",
    "-lnl-3",
    "-lzmq",
    "-lboost_serialization",
    "-luuid",
] + select({
    # Mirrors the YANGMODS conditional on -lyang in common/Makefile.am.
    "//tools/bazel:yang_modules_enabled": ["-lyang"],
    "//conditions:default": [],
})

SWSS_COMMON_DEPS = _APT_DEPS + _YANG_DEPS if IS_BZLMOD else []

SWSS_COMMON_COPTS = [] if IS_BZLMOD else _SYSTEM_COPTS

# Bzlmod dependencies supply declared libraries; WORKSPACE uses system flags.
SWSS_COMMON_LINKOPTS = [] if IS_BZLMOD else _SYSTEM_LINKOPTS
