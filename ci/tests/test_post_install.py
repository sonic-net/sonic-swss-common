import os

import pytest

from buildenv_setup.model import Context, PostInstall
from buildenv_setup.post_install import PostInstallError, resolve_script, select

CTX_BUILD = Context("amd64", "bookworm", "bookworm-container", "build", "master")
CTX_TEST = Context("amd64", "bookworm", "bookworm-container", "test", "master")


def _write(path, body):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(body)


def test_resolve_inline_script_wins():
    entry = PostInstall(name="x", script="echo hi")
    assert resolve_script(entry) == "echo hi"


def test_resolve_source_from_owner(tmp_path):
    owner = tmp_path / "sairedis" / "build-env"
    _write(str(owner / "s.sh"), "local-body")
    entry = PostInstall(name="x", source="s.sh", owner_build_env=str(owner))
    assert resolve_script(entry) == "local-body"


def test_resolve_source_falls_back_to_cascaded_upstream(tmp_path):
    # Consumer declares the entry but ships NO local script; the body is provided by
    # a cascaded upstream bundle's build-env/ and found by basename.
    owner = tmp_path / "sairedis" / "build-env"          # no s.sh here
    upstream = tmp_path / "swss-common" / "build-env"
    _write(str(upstream / "s.sh"), "shared-body")
    entry = PostInstall(name="x", source="s.sh", owner_build_env=str(owner))
    assert resolve_script(entry, search_dirs=[str(upstream)]) == "shared-body"


def test_resolve_local_preferred_over_cascaded(tmp_path):
    owner = tmp_path / "sairedis" / "build-env"
    upstream = tmp_path / "swss-common" / "build-env"
    _write(str(owner / "s.sh"), "local-body")
    _write(str(upstream / "s.sh"), "shared-body")
    entry = PostInstall(name="x", source="s.sh", owner_build_env=str(owner))
    assert resolve_script(entry, search_dirs=[str(upstream)]) == "local-body"


def test_resolve_missing_everywhere_raises(tmp_path):
    entry = PostInstall(name="x", source="s.sh",
                        owner_build_env=str(tmp_path / "build-env"))
    with pytest.raises(PostInstallError):
        resolve_script(entry, search_dirs=[str(tmp_path / "other")])


def test_select_scope_filter_lets_consumer_own_entry_win_at_build():
    # Mirrors the shared-redis dedup: upstream (sonic-swss-common) declares the hook
    # test-scoped; the consumer (sonic-sairedis) re-declares it [build, test]. During
    # a build-scope run the upstream entry is filtered out (so it is NOT marked seen),
    # and the consumer's own entry -- which reuses the cascaded script -- is chosen.
    upstream = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["test"], owner_build_env="/u/build-env")
    consumer = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["build", "test"], owner_build_env="/c/build-env")
    chosen = select([upstream, consumer], CTX_BUILD)   # cascaded first, then local
    assert [e.owner_build_env for e in chosen] == ["/c/build-env"]


def test_select_upstream_entry_wins_at_test_scope():
    # At test scope the upstream (cascaded-first) entry is chosen and marks the name
    # seen, so the consumer's same-named entry is skipped -- no double run.
    upstream = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["test"], owner_build_env="/u/build-env")
    consumer = PostInstall(name="configure-redis-for-tests", source="r.sh",
                           scopes=["build", "test"], owner_build_env="/c/build-env")
    chosen = select([upstream, consumer], CTX_TEST)
    assert [e.owner_build_env for e in chosen] == ["/u/build-env"]
