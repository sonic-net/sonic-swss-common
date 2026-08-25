load("@tar.bzl//tar:extensions.bzl", "create_repositories")
load("@bazel_lib//lib:repositories.bzl", "bazel_lib_dependencies", "bazel_lib_register_toolchains")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@platforms//host:extension.bzl", "host_platform_repo")
     
def tar_setup():
    create_repositories()
    
    native.register_toolchains("@bsd_tar_toolchains//:all")
    
    bazel_lib_dependencies()
    
    bazel_lib_register_toolchains()
    
    maybe(
        host_platform_repo,
        name = "host_platform",
    )
