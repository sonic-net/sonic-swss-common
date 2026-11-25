# load("@rules_cc//cc:cc_test.bzl", "cc_test")
# load("@rules_shell//shell:sh_test.bzl", "sh_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "status_code_util_test",
    srcs = ["status_code_util_test.cpp"],
    deps = [
        "//:libswsscommon",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "saiaclschema_ut",
    srcs = ["saiaclschema_ut.cpp"],
    deps = [
        "//:libswsscommon",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "notification_queue_ut",
    srcs = ["notification_queue_ut.cpp"],
    deps = [
        "//:libswsscommon",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "interface_ut",
    srcs = ["interface_ut.cpp"],
    deps = [
        "//:libswsscommon",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "vrf_ut",
    srcs = ["vrf_ut.cpp"],
    deps = [
        "//:libswsscommon",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "shared_library_runtime_test",
    srcs = ["shared_library_runtime_test.c"],
    linkstatic = False,
    deps = ["//:libswsscommon_shared"],
)

sh_test(
    name = "shared_library_elf_test",
    srcs = ["shared_library_elf_test.sh"],
    args = [
        "$(location //:libswsscommon_consolidated.so)",
        "$(location //sonic-db-cli:libsonicdbcli.so.0)",
    ],
    data = [
        "//:libswsscommon_consolidated.so",
        "//sonic-db-cli:libsonicdbcli.so.0",
    ],
)

cc_binary(
    name = "defaultvalueprovider_ut_binary",
    testonly = True,
    srcs = ["defaultvalueprovider_ut.cpp"],
    linkstatic = False,
    target_compatible_with = select({
        "//tools/bazel:yang_modules_enabled": [],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
    deps = [
        "//:libswsscommon_shared",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

# DefaultValueProvider ignores symlinks while walking the model directory.
# The runner copies declared fixtures to regular files under TEST_TMPDIR.
# Pass each path explicitly so the test also works from an external consumer.
_DEFAULTVALUE_FIXTURES = glob([
    "yang/*.yang",
    "yang-missing-ref/*.yang",
])

sh_test(
    name = "defaultvalueprovider_ut",
    srcs = ["defaultvalueprovider_ut.sh"],
    args = ["$(location :defaultvalueprovider_ut_binary)"] + [
        "$(location %s)" % fixture
        for fixture in _DEFAULTVALUE_FIXTURES
    ],
    data = [":defaultvalueprovider_ut_binary"] + _DEFAULTVALUE_FIXTURES,
    target_compatible_with = select({
        "//tools/bazel:yang_modules_enabled": [],
        "//conditions:default": ["@platforms//:incompatible"],
    }),
)

# Compile the selected test sources from tests/Makefile.am for CodeQL. The
# regular Bazel tests above cover a smaller set that runs without Redis.
cc_library(
    name = "codeql_test_sources",
    testonly = True,
    srcs = glob(
        ["*.cpp"],
        exclude = ["defaultvalueprovider_ut.cpp"],
    ) + select({
        "//tools/bazel:yang_modules_enabled": ["defaultvalueprovider_ut.cpp"],
        "//conditions:default": [],
    }),
    hdrs = ["logger_ut.h"],
    copts = ["-fno-access-control"],
    tags = ["manual"],
    deps = [
        "//:libswsscommon",
        "//sonic-db-cli:sonicdbcli",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@cppzmq",
    ],
)
