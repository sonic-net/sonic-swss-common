from buildenv_setup.model import Context, Package, PackagesFile
from buildenv_setup.planner import _pip_batches, _select_packages

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
