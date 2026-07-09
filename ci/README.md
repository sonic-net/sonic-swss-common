# ci/ — shared build-environment tooling

This directory hosts [`buildenv_setup`](./buildenv_setup/), the shared tool that
sets up the build/test environment for the SONiC dataplane repos
(sonic-swss-common, sonic-sairedis, sonic-swss).

It lives in **sonic-swss-common** because that is the foundational repo at the
root of the dependency cascade — every consumer's CI already checks it out (or
clones it), so hosting the tool here adds no incremental clone cost. It is
versioned with sonic-swss-common's branches: consumers use the copy from the
branch they are building.

* sonic-swss-common's own CI runs it straight from this checkout:
  `PYTHONPATH=$(Build.SourcesDirectory)/ci python3 -m buildenv_setup ...`
* sonic-sairedis / sonic-swss clone sonic-swss-common at the matching branch and
  run `PYTHONPATH=/tmp/sw-common/ci python3 -m buildenv_setup ...`.

## What it does

Given a repo's `build-env/` configuration, `buildenv_setup`:

1. reads the declarative `packages/*.yaml` + `upstream-artifacts.yaml`,
2. resolves the (possibly cascaded) set of apt/pip packages and upstream DEBs/wheels,
3. installs them (apt → upstream DEBs → pip/wheels, so DEBs precede pip), and
4. runs the `post_install` configuration hooks.

See `python3 -m buildenv_setup --help` for all flags, and
[`../build-env/README.md`](../build-env/README.md) for the config schema in use.

## Layout

```
ci/
├── buildenv_setup/       # the tool (multi-module Python package)
│   ├── cli.py            # argparse entrypoint
│   ├── schema.py         # YAML load + validation (fail-loud on unknown fields)
│   ├── predicates.py     # when: evaluation + {var} substitution
│   ├── cascade.py        # upstream-artifact resolution + recursive bundle walk
│   ├── azp_client.py     # Azure DevOps REST artifact download
│   ├── installer.py      # apt/pip/dpkg execution
│   ├── post_install.py   # post_install selection + resolution
│   ├── planner.py        # orchestration + --dry-run rendering
│   ├── topo.py           # requires: topological sort
│   └── model.py          # dataclasses for parsed config
└── tests/                # unit tests (run with pytest)
```

## Schema-evolution policy

The `build-env/` schema is **additive-only** (fields are only added, never
removed/renamed/repurposed) and the tool **fails loud on unknown fields** rather
than silently ignoring them — an unknown field may encode a required setup step,
so dropping it silently would produce a subtly-broken environment. Land tool
support for a new field before any repo's `build-env/` uses it.

## Tests

```bash
cd ci && python3 -m pytest tests/ -q
```
