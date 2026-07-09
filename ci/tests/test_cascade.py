import pytest

from buildenv_setup.cascade import (
    CascadeError,
    ResolvedDeb,
    ResolvedUpstream,
    _subpaths_for,
    collect_bundles,
    resolve_upstream_file,
)
from buildenv_setup.azp_client import ArtifactRef
from buildenv_setup.model import Context, Deb, Upstream, UpstreamFile

BOOK_AMD = Context("amd64", "bookworm", "bookworm-container", "build", "master")
TRIXIE = Context("amd64", "trixie", "trixie-container", "build", "master")
ARMHF = Context("armhf", "bullseye", "bullseye-container", "build", "master")


def _common_libs():
    return Upstream(
        name="common-libs",
        pipeline="Azure.sonic-buildimage.common_libs",
        cascade_optional=True,
        artifact_name=[
            {"when": {"arch": "amd64"}, "value": "common-lib"},
            {"when": {"arch": {"not": "amd64"}}, "value": "common-lib.{arch}"},
        ],
        debs=[
            Deb(path="target/debs/{debian_version}/libyang_1.0*.deb"),
            Deb(path="target/debs/{debian_version}/libpcre*.deb",
                when={"debian_version": "trixie"}),
        ],
    )


def test_resolve_artifact_name_arch_suffix():
    uf = UpstreamFile([_common_libs()])
    amd = resolve_upstream_file(uf, BOOK_AMD)[0]
    arm = resolve_upstream_file(uf, ARMHF)[0]
    assert amd.ref.artifact_name == "common-lib"
    assert arm.ref.artifact_name == "common-lib.armhf"


def test_resolve_deb_when_filter():
    uf = UpstreamFile([_common_libs()])
    book = resolve_upstream_file(uf, BOOK_AMD)[0]
    trix = resolve_upstream_file(uf, TRIXIE)[0]
    # libpcre only appears on trixie
    book_debs = [d.pattern for d in book.debs]
    trix_debs = [d.pattern for d in trix.debs]
    assert "target/debs/bookworm/libpcre*.deb" not in book_debs
    assert "target/debs/trixie/libpcre*.deb" in trix_debs


def test_scope_filter_skips_test_only_upstream():
    up = Upstream(name="test-only", pipeline="p", artifact_name="a", scopes=["test"])
    uf = UpstreamFile([up])
    assert resolve_upstream_file(uf, BOOK_AMD) == []      # build scope skips it
    test_ctx = Context("amd64", "bookworm", "ubuntu-22.04", "test", "master")
    assert len(resolve_upstream_file(uf, test_ctx)) == 1


def test_run_id_passthrough():
    up = Upstream(name="x", pipeline="p", artifact_name="a", run_id=42, cascade_optional=True)
    ru = resolve_upstream_file(UpstreamFile([up]), BOOK_AMD)[0]
    assert ru.ref.run_id == 42


def _ru(debs=(), wheels=()):
    return ResolvedUpstream(
        name="x", ref=ArtifactRef("build", "p", "a", "master"), install_env={},
        debs=[ResolvedDeb(p, [], False) for p in debs], wheels=list(wheels),
        cascade_optional=True,
    )


def test_subpaths_derivation():
    ru = _ru(debs=["target/debs/bookworm/libyang3_*.deb"],
             wheels=["target/python-wheels/bookworm/foo.whl"])
    assert _subpaths_for(ru) == ["target/debs/bookworm", "target/python-wheels/bookworm"]


def test_subpaths_none_when_dir_has_glob():
    # a glob in the directory portion means we can't use a subPath filter
    ru = _ru(debs=["target/debs/*/libyang3_*.deb"])
    assert _subpaths_for(ru) is None


# --------------------------- staged bundles -------------------------------- #

def test_collect_bundles_staged(tmp_path):
    staged = tmp_path / "staged"
    debdir = staged / "common-libs" / "target" / "debs" / "bookworm"
    debdir.mkdir(parents=True)
    (debdir / "libyang_1.0.deb").write_text("x")

    uf = UpstreamFile([Upstream(
        name="common-libs", pipeline="p", cascade_optional=True, artifact_name="common-lib",
        debs=[Deb(path="target/debs/{debian_version}/libyang_1.0*.deb")],
    )])
    arts = collect_bundles(uf, BOOK_AMD, client=None, work_dir=str(tmp_path / "w"),
                           staged_dir=str(staged))
    assert len(arts) == 1
    assert arts[0].deb_files[0].endswith("libyang_1.0.deb")


def test_recursive_glob_fallback(tmp_path):
    # deb is NOT at the pattern's path prefix; recursive-basename fallback should find it
    staged = tmp_path / "staged"
    (staged / "common-libs" / "unexpected").mkdir(parents=True)
    (staged / "common-libs" / "unexpected" / "libyang_1.0.deb").write_text("x")
    uf = UpstreamFile([Upstream(
        name="common-libs", pipeline="p", cascade_optional=True, artifact_name="a",
        debs=[Deb(path="target/debs/bookworm/libyang_1.0*.deb")],
    )])
    arts = collect_bundles(uf, BOOK_AMD, client=None, work_dir=str(tmp_path / "w"),
                           staged_dir=str(staged))
    assert arts[0].deb_files[0].endswith("libyang_1.0.deb")


def test_required_staged_missing_raises(tmp_path):
    uf = UpstreamFile([Upstream(name="x", pipeline="p", artifact_name="a", cascade_optional=True)])
    with pytest.raises(CascadeError):
        collect_bundles(uf, BOOK_AMD, client=None, work_dir=str(tmp_path / "w"),
                        staged_dir=str(tmp_path / "staged"), required_staged={"x"})


def test_missing_glob_raises(tmp_path):
    staged = tmp_path / "staged"
    (staged / "common-libs").mkdir(parents=True)
    uf = UpstreamFile([Upstream(
        name="common-libs", pipeline="p", cascade_optional=True, artifact_name="a",
        debs=[Deb(path="target/debs/bookworm/does-not-exist_*.deb")],
    )])
    with pytest.raises(CascadeError):
        collect_bundles(uf, BOOK_AMD, client=None, work_dir=str(tmp_path / "w"),
                        staged_dir=str(staged))


def test_cascade_recurses_into_nested_bundle(tmp_path):
    # staged 'sairedis' bundle that itself declares an upstream 'sw-common'
    staged = tmp_path / "staged"
    sair = staged / "sairedis"
    (sair).mkdir(parents=True)
    (sair / "libsairedis_1.0.deb").write_text("x")
    nested_be = sair / "build-env"
    nested_be.mkdir()
    (nested_be / "upstream-artifacts.yaml").write_text(
        "upstream:\n"
        "  - name: sw-common\n"
        "    pipeline: p\n"
        "    cascade_optional: true\n"
        "    artifact_name: a\n"
        "    debs: [libswsscommon_1.0.deb]\n"
    )
    swc = staged / "sw-common"
    swc.mkdir()
    (swc / "libswsscommon_1.0.deb").write_text("x")

    uf = UpstreamFile([Upstream(
        name="sairedis", pipeline="p", cascade_optional=True, artifact_name="a",
        debs=[Deb(path="libsairedis_1.0.deb")],
    )])
    arts = collect_bundles(uf, BOOK_AMD, client=None, work_dir=str(tmp_path / "w"),
                           staged_dir=str(staged))
    names = {a.name for a in arts}
    assert names == {"sairedis", "sw-common"}   # cascade pulled in the nested upstream


def test_required_staged_used_only_via_nested_bundle(tmp_path):
    # Regression: a staged upstream referenced ONLY through a NESTED bundle must be
    # recorded as "used" so it doesn't trip the required-staged check. Before the
    # used_staged threading fix, the recursion's used-set was not shared with the
    # top-level call, so 'sw-common' (reached only via sairedis's cascade) was
    # wrongly reported unused.
    staged = tmp_path / "staged"
    sair = staged / "sairedis"
    sair.mkdir(parents=True)
    (sair / "libsairedis_1.0.deb").write_text("x")
    nested_be = sair / "build-env"
    nested_be.mkdir()
    (nested_be / "upstream-artifacts.yaml").write_text(
        "upstream:\n"
        "  - name: sw-common\n"
        "    pipeline: p\n"
        "    cascade_optional: true\n"
        "    artifact_name: a\n"
        "    debs: [libswsscommon_1.0.deb]\n"
    )
    swc = staged / "sw-common"
    swc.mkdir()
    (swc / "libswsscommon_1.0.deb").write_text("x")

    uf = UpstreamFile([Upstream(
        name="sairedis", pipeline="p", cascade_optional=True, artifact_name="a",
        debs=[Deb(path="libsairedis_1.0.deb")],
    )])
    # Requiring 'sw-common' (only reachable via the nested cascade) must NOT raise.
    arts = collect_bundles(uf, BOOK_AMD, client=None, work_dir=str(tmp_path / "w"),
                           staged_dir=str(staged), required_staged={"sw-common"})
    assert {a.name for a in arts} == {"sairedis", "sw-common"}
