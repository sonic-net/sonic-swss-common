"""Generate Common's cfg_schema.h from the registry model inputs."""

_OFFLINE_PYTHON_ENV = {
    "PIP_DISABLE_PIP_VERSION_CHECK": "1",
    "PIP_NO_INDEX": "1",
    "PYTHONDONTWRITEBYTECODE": "1",
    "PYTHONHASHSEED": "0",
    "PYTHONNOUSERSITE": "1",
}

def _cfg_schema_impl(ctx):
    args = ctx.actions.args()
    args.add("-d", ctx.file.models.path)
    args.add("-o", ctx.outputs.out.path)
    ctx.actions.run(
        executable = ctx.executable._generator,
        arguments = [args],
        inputs = [ctx.file.models],
        outputs = [ctx.outputs.out],
        tools = [ctx.attr._generator[DefaultInfo].files_to_run],
        env = _OFFLINE_PYTHON_ENV,
        execution_requirements = {"block-network": "1"},
        mnemonic = "GenCfgSchema",
        progress_message = "Generating cfg_schema.h for %{label}",
    )
    return [DefaultInfo(files = depset([ctx.outputs.out]))]

cfg_schema = rule(
    implementation = _cfg_schema_impl,
    attrs = {
        "models": attr.label(allow_single_file = True, mandatory = True),
        "out": attr.output(mandatory = True),
        "_generator": attr.label(
            default = Label("//tools/bazel/yang:gen_cfg_schema"),
            executable = True,
            cfg = "exec",
        ),
    },
)
