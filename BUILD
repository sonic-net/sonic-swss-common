load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("//:always_link_transition.bzl", "alwayslink_cc_binary")

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

swss_common_hdrs = glob([
    "common/*.h",
    "common/*.hpp",
], allow_empty = True)

swss_common_deps = [
    "@bookworm//libhiredis-dev:libhiredis",
    "@bookworm//nlohmann-json3-dev:nlohmann-json3",
    "@bookworm//libnl-3-dev:libnl-3",
    "@bookworm//libnl-route-3-dev:libnl-route-3",
    "@bookworm//libnl-nf-3-dev:libnl-nf-3",
    "@bookworm//libyang2-dev:libyang2",
    "@bookworm//libzmq3-dev:libzmq3",
    "@bookworm//uuid-dev:uuid",
    "@bookworm//libboost-dev:libboost",
    "@bookworm//libboost-serialization-dev:libboost-serialization",
]

filegroup(
    name = "hdrs",
    srcs = swss_common_hdrs,
)

# Necessary for when we import swss_common as a prebuilt .so file.
# We want all the deps from swsscommon, but none of the symbols.
# The symbols will come from the prebuilt .so file.
# TODO Remove once the cc_shared_library version of swss_common works with rules_distroless.
cc_library(
  name = "common_deps",
  deps = swss_common_deps,
)

# Necessary for when we import swss_common as a dynamic .so file.
# We don't want to include the whole archive when we want to override them with mocks.
# TODO Remove once the cc_shared_library version of swss_common works with rules_distroless.
bool_flag(
    name = "alwayslink",
    build_setting_default = False,
)

config_setting(
  name = "alwayslink_false",
  flag_values = { ":alwayslink": "false" }
)

config_setting(
  name = "alwayslink_true",
  flag_values = { ":alwayslink": "true" }
)

cc_library(
    name = "common",
    srcs = glob(
        ["common/*.cpp"],
        ["common/loglevel.cpp", "common/loglevel_util.cpp"]
    ),
    hdrs = swss_common_hdrs,
    copts = [
        "-fPIC",
        "-std=c++14",
        # TODO: this is not required with apt.installed debs.
        # "-I/usr/include/libnl3", # Expected location in the SONiC build container"
    ],
    # Not needed with apt.install
    # linkopts = ["-lpthread -lhiredis -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lzmq -luuid -lyang"],
    includes = [
        "common",
    ],
    linkopts = ["-lboost_serialization"],
    # Approach 1:
    deps = swss_common_deps,
    # Approach 2: BCR entries compiled from source
    # deps = [
    #     "@boost.algorithm",
    #     "@boost.serialization",
    #     "@nlohmann_json//:json",
    #     "@libuuid//:libuuid",
    #     "@swig//:swig",
    # ],
    visibility = ["//visibility:public"],
    # Force all symbols to be included when linking into shared library
    # This is required for the consolidated .so to export all symbols needed by SWIG bindings
    # However, some builds like `sonic-swss` will refuse to link if they are always linked,
    # because symbols in `libcommon.a` will conflict with test mocks. Hence, the select()
    alwayslink = select({
      ":alwayslink_true": True,
      ":alwayslink_false": False,
    }),
)

cc_library(
    name = "libswsscommon",
    hdrs = glob([
        "common/*.h",
        "common/*.hpp",
    ], allow_empty = True),
    include_prefix = "swss",
    strip_include_prefix = "common",
    deps = [":common"],
)

# Consolidated shared library for cgo to avoid argument list too long
# The :common library has alwayslink=True to ensure all symbols are exported
cc_binary(
    name = "libswsscommon_consolidated_base",
    srcs = [
        # Include static libraries directly to force static linking and bypass linker scripts
        "@@rules_distroless++apt+bookworm_libbsd-dev-amd64_0.11.7-2//:usr/lib/x86_64-linux-gnu/libbsd.a",
        "@@rules_distroless++apt+bookworm_libmd-dev-amd64_1.0.4-2//:usr/lib/x86_64-linux-gnu/libmd.a",
    ],
    linkshared = True,
    linkopts = [
        "-static-libstdc++",
        "-static-libgcc",
        # Allow undefined symbols from external runtime deps (hiredis, zmq, etc.)
        # These are resolved at runtime when the .so is loaded
        "-Wl,--allow-shlib-undefined",
        "-Wl,--undefined-version",
        # Exclude libbsd from dynamic linking - we use static .a files above
        "-Wl,--exclude-libs,libbsd.a",
        "-Wl,--exclude-libs,libmd.a",
    ],
    deps = [":common"],
)

# If we're building the consolidated binary, we always want to link it.
alwayslink_cc_binary(
  name = "libswsscommon_consolidated.so",
  binary = "libswsscommon_consolidated_base",
)


# Alias for compatibility with existing references
alias(
    name = "swsscommon_base",
    actual = ":common",
)

# Filegroups for swig bindings and other assets
filegroup(
    name = "all_hdrs",
    srcs = glob([
        "common/*.h",
        "common/*.hpp",
    ], allow_empty = True),
)

filegroup(
    name = "swig_template",
    srcs = ["//pyext:swsscommon.i"],
)

filegroup(
    name = "all_luas",
    srcs = glob(["common/*.lua"]),
)

# TODO(bazel-ready): CFLAGS_COMMON is not respected yet in many of these targets.
