# Common schema generation

This package runs Common's unchanged `gen_cfg_schema.py` to generate
`cfg_schema.h`. Reusable YANG libraries, model preparation, dependency packages,
and their standalone tests belong to the SONiC Bazel registry.

## Source contract

`MODULE.bazel` selects these registry modules:

- [`sonic-yang-models` `1.0.1-dd732f9512a84966487951250adae8e3e5c9943e`](https://github.com/sonic-net/sonic-bazel-registry/tree/main/modules/sonic-yang-models/1.0.1-dd732f9512a84966487951250adae8e3e5c9943e)
  owns the production model sources/templates, setup manifest, preparation
  action, runtime package, and model/package checks. Common consumes its
  `@sonic_yang_models//:yang_models` directory.
- [`sonic-yang-mgmt` `1.0.0-9ab452d22773c41783092ae3bd6c206d6c257c8d`](https://github.com/sonic-net/sonic-bazel-registry/blob/main/modules/sonic-yang-mgmt/1.0.0-9ab452d22773c41783092ae3bd6c206d6c257c8d/README.md)
  supplies the `sonic_yang` Python modules through
  `@sonic_yang_mgmt//:sonic_yang_mgmt`. It owns the library's Python dependencies
  and its standalone runtime test.
- Native [`libyang` `3.12.2.sonic.9`](https://github.com/sonic-net/sonic-bazel-registry/tree/main/modules/libyang/3.12.2.sonic.9)
  supplies matching headers, `libyang.so.3`, and runtime/debug packages. It
  preserves the production `LYD_VALIDATE_NOEXTDEPS` patch and large-file support;
  PCRE2 and xxHash are source-built static implementation dependencies.

The models and management modules use the buildimage revisions encoded in
their respective registry versions.
The management module depends on registry
[`libyang-python` `3.1.0.sonic.7`](https://github.com/sonic-net/sonic-bazel-registry/blob/main/modules/libyang-python/3.1.0.sonic.7/README.md),
which builds CESNET/libyang-python `v3.1.0` with the four patches used by the
production `src/libyang3-py3/Makefile`. The registry owns CFFI generation and the
binding's dependencies and tests. This differs from standalone Azure's
[`build-env/packages/base.yaml`](../../../build-env/packages/base.yaml), which
installs `libyang==3.3.0` from pip after native libyang packages.

## Common's generation action

`cfg_schema` passes the registry's prepared directory to this repository's
`gen_cfg_schema.py`. The declared Python 3.13 generator and its management
library run on the execution platform, including their CFFI and native libyang
inputs. Common's compiled library and packages use the target platform.

The action declares the model directory and generator runfiles as inputs,
requests network blocking, and disables package-installer network access.
Dependency sources and locked Python wheels are fetched during Bazel resolution.
The `../swig.bzl` interface adapter preserves Common's enabled/disabled feature
defines and generated Python/Go API behavior.

## Deployment and validation

Common publishes its own runtime, debug, CLI, and Python archives. Enabled
deployments additionally need the model and libyang runtime packages supplied
by the [registry workflows](https://github.com/sonic-net/sonic-bazel-registry/actions):
`@sonic_yang_models//:yang_models_pkg`, `@libyang//:libyang_pkg`, and libyang's
matching `@libyang//:libyang_pkg.debug_symbols`. Use the selected module versions
and deployment architecture. Download the matching
`registry-ci-<module>-<version>-<architecture>` artifact (`amd64` or `arm64`)
from **Registry CI** and use `outputs.json` to locate the files under `outputs/`.
Keep the runtime/debug pair from the same run. The model package installs
`/usr/local/yang-models`; production Make provides that payload through the
`sonic_yang_models` wheel.

Registry CI owns each dependency's package/runtime tests. Common CI retains
schema generation, feature fixtures, and Common's runtime/package tests. Its
Python and Go tests stage the pinned libyang package, preload the exact extracted
SONAME, and check loaded paths. This ensures the enabled mode uses the packaged
dependency and the disabled mode does not load it.

When updating inputs, compare the generated header with the existing production
contract and run Common's enabled/disabled target lists in
[`docs/bazel.md`](../../../docs/bazel.md). Registry module checks validate the
reusable model payload and library independently.
