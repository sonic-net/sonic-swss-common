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
    ]),
    copts = [
        "-std=c++14",
        "-I/usr/include/libnl3", # Expected location in the SONiC build container"
    ],
    includes = [
        "common",
    ],
    # deps = [
    #     "@boost.algorithm",
    #     "@boost.serialization",
    #     "@nlohmann_json//:json",
    #     "@libuuid//:libuuid",
    #     "@swig//:swig",
    # ],
    linkopts = ["-lpthread -lhiredis -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lzmq -luuid -lyang"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "libswsscommon",
    hdrs = glob([
        "common/*.h",
        "common/*.hpp",
    ]),
    include_prefix = "swss",
    strip_include_prefix = "common",
    deps = [":common"],
)
