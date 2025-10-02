package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "common",
    srcs = glob(
        ["common/*.cpp"],
        ["common/loglevel.cpp", "common/loglevel_util.cpp"]
    ),
    hdrs = glob([
        "common/*.h",
        "common/*.hpp",
    ], allow_empty = True),
    copts = [
        "-std=c++14",
        # TODO: this is not required with apt.installed debs.
        # "-I/usr/include/libnl3", # Expected location in the SONiC build container"
    ],
    # Not needed with apt.install
    # linkopts = ["-lpthread -lhiredis -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lzmq -luuid -lyang"],
    includes = [
        "common",
    ],
    # Approach 1:
    deps = [
        "@focal//libhiredis-dev:libhiredis",
        "@focal//nlohmann-json3-dev:nlohmann-json3",
        "@focal//libnl-3-dev:libnl-3",
        "@focal//libnl-route-3-dev:libnl-route-3",
        "@focal//libnl-nf-3-dev:libnl-nf-3",
        "@focal//libyang-dev:libyang",
        "@focal//libzmq3-dev:libzmq3",
        "@focal//uuid-dev:uuid",
        "@focal//libboost-dev:libboost"
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
