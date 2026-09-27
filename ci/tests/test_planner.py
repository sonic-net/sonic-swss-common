from buildenv_setup.model import Context, Package, PackagesFile
from buildenv_setup.planner import (
    _collect_cascaded_config,
    _deb_install_groups,
    _pip_batches,
    _select_packages,
    PlannerError,
)
from buildenv_setup.cascade import InstalledArtifact

import textwrap

import pytest

CTX = Context("amd64", "bookworm", "bookworm-container", "build", "master")


def _pf(packages):
    return PackagesFile(packages=packages)


def test_select_splits_apt_and_pip_and_dedups():
    files = [
        _pf([Package(name="libhiredis-dev"), Package(name="pytest", type="pip")]),
        _pf([Package(name="libhiredis-dev"), Package(name="Pympler==0.8", type="pip")]),
    ]
    apt, pip = _select_packages(files, [], CTX)
    assert apt == ["libhiredis-dev"]                        # deduped
    assert [name for name, _ in pip] == ["pytest", "Pympler==0.8"]


def test_select_when_filter():
    files = [_pf([
        Package(name="always"),
        Package(name="arm-only", when={"arch": {"not": "amd64"}}),
    ])]
    apt, _ = _select_packages(files, [], CTX)
    assert apt == ["always"]


def test_select_requires_ordering():
    files = [_pf([
        Package(name="libyang", type="pip", requires=["python3-cffi"]),
        Package(name="python3-cffi", type="pip"),
    ])]
    _, pip = _select_packages(files, [], CTX)
    names = [name for name, _ in pip]
    assert names.index("python3-cffi") < names.index("libyang")


def test_cascaded_packages_come_first():
    cascaded = [Package(name="from-upstream")]
    files = [_pf([Package(name="local")])]
    apt, _ = _select_packages(files, cascaded, CTX)
    assert apt == ["from-upstream", "local"]


def test_pip_batches_groups_plain_and_splits_args():
    batches = _pip_batches([("a", ()), ("b", ()), ("libyang", ("--no-build-isolation",))])
    assert (["a", "b"], ()) in batches
    assert (["libyang"], ("--no-build-isolation",)) in batches


def test_pip_batches_preserve_order_plain_requires_args():
    # requires:-toposort puts the args pip first because the plain pip depends on
    # it; _pip_batches must keep that order (args pip installed before plain pip).
    batches = _pip_batches([("argspip", ("--flag",)), ("plainpip", ())])
    assert batches == [(["argspip"], ("--flag",)), (["plainpip"], ())]


def test_pip_batches_preserve_order_args_requires_plain():
    # Mirror case: args pip depends on a plain pip -> toposort emits the plain pip
    # first; the pending plain batch must be flushed before the args batch.
    batches = _pip_batches([("plainpip", ()), ("argspip", ("--flag",))])
    assert batches == [(["plainpip"], ()), (["argspip"], ("--flag",))]


def test_pip_batches_interleaved_preserves_sequence():
    batches = _pip_batches([("a", ()), ("x", ("--f",)), ("b", ()), ("c", ())])
    assert batches == [(["a"], ()), (["x"], ("--f",)), (["b", "c"], ())]


def _write_cascaded_base(tmp_path, body: str) -> str:
    build_env = tmp_path / "build-env"
    (build_env / "packages").mkdir(parents=True)
    (build_env / "packages" / "base.yaml").write_text(textwrap.dedent(body))
    return str(tmp_path)


def test_cascaded_package_with_apt_source_fails_loud(tmp_path):
    bundle = _write_cascaded_base(tmp_path, """
        apt_sources:
          - name: llvm
            list_url: https://apt.llvm.org/x.list
            gpg_key_url: https://apt.llvm.org/key.asc
        packages:
          - { name: clang-18, type: apt, apt_source: llvm }
    """)
    with pytest.raises(PlannerError) as ei:
        _collect_cascaded_config([bundle], set(), CTX)
    assert "clang-18" in str(ei.value)
    assert "apt_source" in str(ei.value)


def test_cascaded_apt_sources_declaration_fails_loud(tmp_path):
    # Even an apt_sources declaration with no referencing package is rejected:
    # it signals reliance on unsupported cascaded-apt_source behavior.
    bundle = _write_cascaded_base(tmp_path, """
        apt_sources:
          - name: llvm
            list_url: https://apt.llvm.org/x.list
            gpg_key_url: https://apt.llvm.org/key.asc
        packages:
          - { name: build-essential, type: apt }
    """)
    with pytest.raises(PlannerError) as ei:
        _collect_cascaded_config([bundle], set(), CTX)
    assert "apt_sources" in str(ei.value)


def test_cascaded_base_without_apt_source_ok(tmp_path):
    bundle = _write_cascaded_base(tmp_path, """
        packages:
          - { name: libnl-3-dev, type: apt }
          - { name: libyang, type: pip }
    """)
    pkgs, post = _collect_cascaded_config([bundle], set(), CTX)
    assert [p.name for p in pkgs] == ["libnl-3-dev", "libyang"]
    assert post == []


def test_deb_groups_merge_plain_upstreams_into_one_call():
    # Cross-artifact dependency: libswsscommon (sonic-swss-common) depends on
    # libnl/libyang3 (common-libs). Both have empty install_env, so they must be
    # installed in a SINGLE dpkg -i call regardless of declaration order.
    swss_common = InstalledArtifact(
        name="sonic-swss-common", bundle_dir="/b/swss",
        deb_files=["/b/swss/libswsscommon_1.0.0_amd64.deb"],
    )
    common_libs = InstalledArtifact(
        name="common-libs", bundle_dir="/b/cl",
        deb_files=["/b/cl/libnl-nf.deb", "/b/cl/libyang3.deb"],
    )
    groups = _deb_install_groups([swss_common, common_libs])
    assert len(groups) == 1
    assert set(groups[0]["files"]) == {
        "/b/swss/libswsscommon_1.0.0_amd64.deb", "/b/cl/libnl-nf.deb", "/b/cl/libyang3.deb"
    }


def test_deb_groups_preserve_dependency_first_artifact_order():
    # collect_bundles supplies dependency-first order. Different install_env
    # signatures split calls without reordering them.
    plain = InstalledArtifact(
        name="common-libs", bundle_dir="/b/cl", deb_files=["/b/cl/libyang3.deb"],
    )
    vpp = InstalledArtifact(
        name="vpp", bundle_dir="/b/vpp", deb_files=["/b/vpp/vpp.deb", "/b/vpp/libvppinfra.deb"],
        install_env={"VPP_INSTALL_SKIP_SYSCTL": "1"},
        deb_opts={"/b/vpp/vpp.deb": ([], True)},
    )
    groups = _deb_install_groups([plain, vpp])
    assert len(groups) == 2
    assert groups[0]["files"] == ["/b/cl/libyang3.deb"]
    assert groups[0]["env"] == {}
    assert set(groups[1]["files"]) == {"/b/vpp/vpp.deb", "/b/vpp/libvppinfra.deb"}
    assert groups[1]["env"] == {"VPP_INSTALL_SKIP_SYSCTL": "1"}
    assert groups[1]["fix"] is True                         # apt_fix_broken ORed in


def test_deb_groups_union_dpkg_args_within_group():
    art = InstalledArtifact(
        name="x", bundle_dir="/b/x",
        deb_files=["/b/x/a.deb", "/b/x/b.deb"],
        deb_opts={
            "/b/x/a.deb": (["--force-confask"], False),
            "/b/x/b.deb": (["--force-confnew"], False),
        },
    )
    groups = _deb_install_groups([art])
    assert len(groups) == 1
    assert groups[0]["args"] == ["--force-confask", "--force-confnew"]


def test_deb_groups_preserve_insertion_order_among_env_groups():
    # Every environment boundary preserves dependency-first insertion order,
    # including an empty environment after special-environment dependencies.
    envA = InstalledArtifact(name="a", bundle_dir="/a", deb_files=["/a/a.deb"],
                             install_env={"A": "1"})
    envB = InstalledArtifact(name="b", bundle_dir="/b", deb_files=["/b/b.deb"],
                             install_env={"B": "1"})
    plain = InstalledArtifact(name="p", bundle_dir="/p", deb_files=["/p/p.deb"])
    groups = _deb_install_groups([envA, envB, plain])
    assert groups[0]["env"] == {"A": "1"}
    assert groups[1]["env"] == {"B": "1"}
    assert groups[2]["files"] == ["/p/p.deb"]
    assert groups[2]["env"] == {}


def test_deb_groups_split_same_env_around_special_dependency():
    # Regression: sairedis (empty env) depends on VPP (special env), while both
    # depend on ordinary providers. Global grouping by env used to collapse the
    # provider + sairedis DEBs into one early call, so apt repair removed
    # libsaivs before VPP was installed. Preserve the dependency layers.
    providers = InstalledArtifact(
        name="common-libs", bundle_dir="/p", deb_files=["/p/libnl.deb"],
    )
    vpp = InstalledArtifact(
        name="vpp", bundle_dir="/v", deb_files=["/v/vpp.deb"],
        install_env={"VPP_INSTALL_SKIP_SYSCTL": "1"},
    )
    sairedis = InstalledArtifact(
        name="sonic-sairedis", bundle_dir="/s", deb_files=["/s/libsaivs.deb"],
    )

    groups = _deb_install_groups([providers, vpp, sairedis])

    assert [g["files"] for g in groups] == [
        ["/p/libnl.deb"],
        ["/v/vpp.deb"],
        ["/s/libsaivs.deb"],
    ]
    assert [g["env"] for g in groups] == [
        {},
        {"VPP_INSTALL_SKIP_SYSCTL": "1"},
        {},
    ]


def test_deb_groups_ignore_wheel_only_artifact_between_same_env_debs():
    providers = InstalledArtifact(
        name="provider", bundle_dir="/p", deb_files=["/p/provider.deb"],
    )
    wheels = InstalledArtifact(
        name="wheels", bundle_dir="/w", wheel_files=["/w/package.whl"],
    )
    dependent = InstalledArtifact(
        name="dependent", bundle_dir="/d", deb_files=["/d/dependent.deb"],
    )

    groups = _deb_install_groups([providers, wheels, dependent])

    assert len(groups) == 1
    assert groups[0]["files"] == ["/p/provider.deb", "/d/dependent.deb"]
