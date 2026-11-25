"""Generate the Common C API input and Cargo OUT_DIR for Rust bindings."""

def _c_api_header_impl(ctx):
    names = sorted([header.basename for header in ctx.files._headers])
    if not names:
        fail("Common's C API header set is empty")

    out = ctx.actions.declare_file(ctx.label.name + ".h")
    ctx.actions.write(
        output = out,
        content = "\n".join([
            "#include <swss/c-api/%s>" % name
            for name in names
        ]) + "\n",
    )
    return [DefaultInfo(files = depset([out]))]

c_api_header = rule(
    implementation = _c_api_header_impl,
    attrs = {
        "_headers": attr.label(
            default = Label("//common:c_api_hdrs"),
        ),
    },
)

def _bindings_directory_impl(ctx):
    out = ctx.actions.declare_directory(ctx.label.name)
    args = ctx.actions.args()
    args.add(ctx.file.src)
    args.add_all([out], expand_directories = False)

    ctx.actions.run(
        executable = ctx.attr._stage_bindings[DefaultInfo].files_to_run,
        inputs = [ctx.file.src],
        outputs = [out],
        arguments = [args],
        mnemonic = "StageRustBindings",
        progress_message = "Staging Rust bindings for %{label}",
    )

    return [DefaultInfo(files = depset([out]))]

bindings_directory = rule(
    implementation = _bindings_directory_impl,
    attrs = {
        "src": attr.label(allow_single_file = [".rs"], mandatory = True),
        "_stage_bindings": attr.label(
            default = Label("//tools/bazel/rust:stage_bindings"),
            executable = True,
            cfg = "exec",
        ),
    },
)
