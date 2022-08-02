package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "common",
    srcs = glob(["common/*.cpp"], exclude=["common/loglevel.cpp"]),
    hdrs = glob([
        "common/*.h",
        "common/*.hpp",
    ]),
    copts = [
        "-I/usr/include/libnl3", # Expected location in the SONiC build container"
    ],
    includes = [
        "common",
    ],
    linkopts = ["-lpthread -lhiredis -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lzmq -lboost_serialization -luuid"],
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
