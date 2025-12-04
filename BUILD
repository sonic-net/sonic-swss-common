package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

swss_common_hdrs = glob([
    "common/*.h",
    "common/*.hpp",
], allow_empty = True)

filegroup(
    name = "hdrs",
    srcs = swss_common_hdrs,
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
    deps = [
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
    ],
    # Approach 2: BCR entries compiled from source
    # deps = [
    #     "@boost.algorithm",
    #     "@boost.serialization",
    #     "@nlohmann_json//:json",
    #     "@libuuid//:libuuid",
    #     "@swig//:swig",
    # ],
    visibility = ["//visibility:public"],
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
