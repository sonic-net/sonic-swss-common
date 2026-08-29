import pytest

from buildenv_setup.topo import CycleError, toposort


def test_preserves_order_without_deps():
    assert toposort(["a", "b", "c"], {}) == ["a", "b", "c"]


def test_requires_ordering():
    # c requires a; a must come before c
    out = toposort(["c", "b", "a"], {"c": ["a"]})
    assert out.index("a") < out.index("c")


def test_requires_outside_items_ignored():
    # 'redis-server' isn't in the item set (installed elsewhere) -> ignored, no error
    assert toposort(["x"], {"x": ["redis-server"]}) == ["x"]


def test_cycle_raises():
    with pytest.raises(CycleError):
        toposort(["a", "b"], {"a": ["b"], "b": ["a"]})
