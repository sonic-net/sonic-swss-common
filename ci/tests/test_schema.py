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


# --------------------- list/map validation (fail-loud) ---------------------- #

def test_package_pip_args_must_be_list(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - {name: libyang, type: pip, pip_args: "--no-build-isolation"}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_package_requires_must_be_list(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - {name: libyang, type: pip, requires: libyang3}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_post_install_scopes_must_be_list(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        post_install:
          - {name: x, script: "echo hi", scopes: build}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_upstream_deb_dpkg_args_must_be_list(tmp_path):
    path = _write(tmp_path, "up.yaml", """
        upstream:
          - name: u
            pipeline: p
            artifact_name: a
            debs:
              - {path: 'x_*.deb', dpkg_args: "--force-confnew"}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)


def test_upstream_deb_scopes_must_be_list(tmp_path):
    path = _write(tmp_path, "up.yaml", """
        upstream:
          - name: u
            pipeline: p
            artifact_name: a
            debs:
              - {path: 'x_*.deb', scopes: test}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)


def test_upstream_install_env_must_be_mapping(tmp_path):
    path = _write(tmp_path, "up.yaml", """
        upstream:
          - name: u
            pipeline: p
            artifact_name: a
            install_env: [VPP_INSTALL_SKIP_SYSCTL=1]
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)


def test_upstream_install_env_mapping_ok(tmp_path):
    path = _write(tmp_path, "up.yaml", """
        upstream:
          - name: vpp
            pipeline: p
            artifact_name: a
            install_env: {VPP_INSTALL_SKIP_SYSCTL: "1"}
    """)
    uf = schema.load_upstream_file(path)
    assert uf.upstreams[0].install_env == {"VPP_INSTALL_SKIP_SYSCTL": "1"}


# ------------------------- missing-field errors ----------------------------- #

def test_top_level_must_be_mapping(tmp_path):
    path = _write(tmp_path, "base.yaml", "- just\n- a\n- list\n")
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_apt_source_missing_field(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        apt_sources:
          - {name: dotnet, list_url: u}
        packages: []
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_package_missing_name(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - {type: pip}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_package_invalid_type(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        packages:
          - {name: x, type: snap}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_post_install_missing_name(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        post_install:
          - {script: "echo hi"}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_post_install_source_and_script_mutually_exclusive(tmp_path):
    path = _write(tmp_path, "base.yaml", """
        post_install:
          - {name: x, source: s.sh, script: "echo hi"}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_packages_file(path, str(tmp_path))


def test_upstream_missing_required_field(tmp_path):
    path = _write(tmp_path, "up.yaml", """
        upstream:
          - {name: u}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)


def test_upstream_wheel_missing_path(tmp_path):
    path = _write(tmp_path, "up.yaml", """
        upstream:
          - name: u
            pipeline: p
            artifact_name: a
            wheels:
              - {when: {arch: amd64}}
    """)
    with pytest.raises(schema.SchemaError):
        schema.load_upstream_file(path)
