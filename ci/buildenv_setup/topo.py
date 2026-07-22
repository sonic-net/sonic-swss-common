"""Stable topological sort used to order install steps by ``requires:`` edges.

Given a list of item names and a ``requires`` map (item -> list of names that
must come first), return the items in an order that respects the edges while
otherwise preserving the original (declaration) order. Raises :class:`CycleError`
on a cycle so misconfiguration fails fast.
"""

from __future__ import annotations

from typing import Dict, Iterable, List


class CycleError(ValueError):
    pass


def toposort(items: Iterable[str], requires: Dict[str, Iterable[str]]) -> List[str]:
    order = list(items)
    index = {name: i for i, name in enumerate(order)}

    # Only consider edges between items we actually have (ignore requires that
    # point at packages installed in an earlier group, e.g. an apt dep of a pip
    # package handled by group ordering).
    deps: Dict[str, List[str]] = {
        name: [d for d in requires.get(name, []) if d in index] for name in order
    }

    visited: Dict[str, int] = {}   # 0 = visiting, 1 = done
    result: List[str] = []

    def visit(name: str, stack: List[str]) -> None:
        state = visited.get(name)
        if state == 1:
            return
        if state == 0:
            cycle = " -> ".join(stack + [name])
            raise CycleError(f"dependency cycle: {cycle}")
        visited[name] = 0
        # Visit dependencies in declaration order for determinism.
        for dep in sorted(deps[name], key=lambda d: index[d]):
            visit(dep, stack + [name])
        visited[name] = 1
        result.append(name)

    for name in order:
        visit(name, [])
    return result
