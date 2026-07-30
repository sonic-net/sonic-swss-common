from buildenv_setup.model import Context, Package, PackagesFile
from buildenv_setup.planner import _deb_install_groups, _pip_batches, _select_packages
from buildenv_setup.cascade import InstalledArtifact

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


def test_deb_groups_split_by_install_env_plain_first():
    # vpp carries install_env (VPP_INSTALL_SKIP_SYSCTL) + apt_fix_broken, so it is
    # a separate group installed AFTER the empty-install_env providers.
    plain = InstalledArtifact(
        name="common-libs", bundle_dir="/b/cl", deb_files=["/b/cl/libyang3.deb"],
    )
    vpp = InstalledArtifact(
        name="vpp", bundle_dir="/b/vpp", deb_files=["/b/vpp/vpp.deb", "/b/vpp/libvppinfra.deb"],
        install_env={"VPP_INSTALL_SKIP_SYSCTL": "1"},
        deb_opts={"/b/vpp/vpp.deb": ([], True)},
    )
    groups = _deb_install_groups([vpp, plain])   # declared vpp-first on purpose
    assert len(groups) == 2
    assert groups[0]["files"] == ["/b/cl/libyang3.deb"]     # empty-env group first
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
