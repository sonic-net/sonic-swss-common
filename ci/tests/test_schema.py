import textwrap

import pytest

from buildenv_setup import schema


def _write(tmp_path, name, text):
    p = tmp_path / name
    p.write_text(textwrap.dedent(text))
    return str(p)


# --------------------------- packages/*.yaml -------------------------------- #

def test_packages_bare_and_mapping(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - libhiredis-dev
          - {name: pytest, type: pip}
          - {name: libnl-3-dev, when: {arch: {not: amd64}}}
    """)
    pf = schema.load_packages_file(path, str(tmp_path))
    assert [p.name for p in pf.packages] == ["libhiredis-dev", "pytest", "libnl-3-dev"]
    assert pf.packages[0].type == "apt"
    assert pf.packages[1].type == "pip"
    assert pf.packages[2].when == {"arch": {"not": "amd64"}}


def test_post_install_source_xor_script(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        post_install:
          - {name: bad, source: x.sh, script: "echo hi"}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_post_install_records_owner(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        post_install:
          - {name: redis, source: configure-redis.sh, requires: [redis-server], scopes: [build, test]}
    """)
    pf = schema.load_packages_file(path, "/some/build-env")
    entry = pf.post_install[0]
    assert entry.owner_build_env == "/some/build-env"
    assert entry.scopes == ["build", "test"]


def test_invalid_pip_args_type_field(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - {name: bogus, type: conda}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


# ------------------- fail-loud on unknown fields (F3) ----------------------- #

def test_unknown_package_field_raises(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - {name: foo, typ: pip}
    """)
    with pytest.raises(schema.SchemaError) as exc:
        schema.load_packages_file(path, str(tmp_path))
    assert "typ" in str(exc.value)


def test_unknown_toplevel_field_raises(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages: [libhiredis-dev]
        postinstall: []
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


# --------------------------- upstream-artifacts.yaml ------------------------ #

def test_upstream_parse_and_run_id(tmp_path):
    path = _write(tmp_path, "upstream-artifacts.yaml", """
        upstream:
          - name: common-libs
            pipeline: Azure.sonic-buildimage.common_libs
            run_id: 926659
            cascade_optional: true
            artifact_name: common-lib
            debs:
              - target/debs/{debian_version}/libyang_1.0*.deb
    """)
    uf = schema.load_upstream_file(path)
    up = uf.upstreams[0]
    assert up.run_id == 926659
    assert up.cascade_optional is True
    assert up.debs[0].path == "target/debs/{debian_version}/libyang_1.0*.deb"


def test_duplicate_upstream_name_raises(tmp_path):
    path = _write(tmp_path, "upstream-artifacts.yaml", """
        upstream:
          - {name: dup, pipeline: p, artifact_name: a}
          - {name: dup, pipeline: q, artifact_name: b}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)


def test_upstream_missing_required_raises(tmp_path):
    path = _write(tmp_path, "upstream-artifacts.yaml", """
        upstream:
          - {name: x}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)
