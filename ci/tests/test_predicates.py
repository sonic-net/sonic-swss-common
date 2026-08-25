import pytest

from buildenv_setup.model import Context
from buildenv_setup.predicates import PredicateError, evaluate, substitute

CTX = Context(arch="amd64", debian_version="bookworm", host_os="bookworm-container",
              scope="build", branch="master")


def test_empty_when_is_true():
    assert evaluate(None, CTX) is True
    assert evaluate({}, CTX) is True


def test_scalar_match():
    assert evaluate({"arch": "amd64"}, CTX) is True
    assert evaluate({"arch": "armhf"}, CTX) is False


def test_list_match():
    assert evaluate({"arch": ["amd64", "armhf"]}, CTX) is True
    assert evaluate({"arch": ["arm64", "armhf"]}, CTX) is False


def test_negation():
    assert evaluate({"arch": {"not": "armhf"}}, CTX) is True
    assert evaluate({"arch": {"not": "amd64"}}, CTX) is False


def test_and_across_keys():
    assert evaluate({"arch": "amd64", "debian_version": "bookworm"}, CTX) is True
    assert evaluate({"arch": "amd64", "debian_version": "trixie"}, CTX) is False


def test_unknown_key_raises():
    with pytest.raises(PredicateError):
        evaluate({"distro": "debian"}, CTX)


def test_substitute():
    assert substitute("common-lib.{arch}", CTX) == "common-lib.amd64"
    assert substitute("target/debs/{debian_version}/x.deb", CTX) == "target/debs/bookworm/x.deb"
    assert substitute("no-vars", CTX) == "no-vars"
