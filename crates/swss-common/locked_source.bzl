"""Compile the original host-services Rust source against its caller-owned lock.

The default 4b86a770 build.rs uses installed Common C API headers. Keep that
source/bindings/C++ interface here; the caller owns its five Cargo dependencies.
"""

load("@rules_rs//rs:rust_library.bzl", "rust_library")

_BINDINGS = Label("//crates/swss-common:bindings_dir")
_SOURCES = Label("@swss_common_4b86a770_src//:rust_sources")
_ROOT = Label("@swss_common_4b86a770_src//:src/lib.rs")
_NATIVE = Label("//:libswsscommon_shared")

def swss_common_4b86a770(name, getset, lazy_static, libc, serde, tracing_subscriber, visibility = None):
    """Instantiate the fixed crate with the caller's original Cargo dependency graph.

    Args:
        name: Local target name.
        getset: Original lock's getset proc-macro crate label.
        lazy_static: Original lock's lazy_static crate label.
        libc: Original lock's libc crate label.
        serde: Original lock's serde crate label, shared with its consumers.
        tracing_subscriber: Original lock's tracing-subscriber crate label.
        visibility: Visibility of the caller-owned instance.
    """
    rust_library(
        name = name,
        crate_name = "swss_common",
        crate_root = _ROOT,
        srcs = [_SOURCES],
        compile_data = [_BINDINGS],
        edition = "2021",
        version = "0.1.0",
        rustc_env = {"OUT_DIR": "$(execpath %s)" % _BINDINGS},
        rustc_flags = ["-Cdebuginfo=2"],
        link_deps = [_NATIVE],
        deps = [getset, lazy_static, libc, serde, tracing_subscriber],
        visibility = visibility,
    )
