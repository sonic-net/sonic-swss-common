"""``when:`` predicate evaluation and ``{var}`` template substitution.

Predicate grammar (matches the design doc):

* omitted / empty ``when``            -> always true
* ``{arch: amd64}``                   -> scalar equality
* ``{arch: [amd64, armhf]}``          -> any-of (list membership)
* ``{arch: {not: amd64}}``            -> negation
* ``{arch: amd64, debian_version: trixie}`` -> AND across keys

Supported keys: ``arch``, ``debian_version``, ``host_os``, ``scope``, ``branch``.
Unknown keys raise :class:`PredicateError` (fail-fast on typos).
"""

from __future__ import annotations

from typing import Any

from .model import Context

_KEYS = ("arch", "debian_version", "host_os", "scope", "branch")


class PredicateError(ValueError):
    pass


def _match(matcher: Any, actual: str) -> bool:
    if isinstance(matcher, dict):
        if set(matcher) - {"not"}:
            raise PredicateError(f"unsupported matcher keys: {sorted(matcher)}")
        if "not" in matcher:
            return not _match(matcher["not"], actual)
        return True  # empty dict matches anything
    if isinstance(matcher, (list, tuple)):
        return actual in [str(m) for m in matcher]
    return actual == str(matcher)


def evaluate(when: Any, ctx: Context) -> bool:
    """Return True if ``when`` predicate holds for ``ctx``."""
    if not when:
        return True
    if not isinstance(when, dict):
        raise PredicateError(f"when: must be a mapping, got {type(when).__name__}")
    for key, matcher in when.items():
        if key not in _KEYS:
            raise PredicateError(f"unknown predicate key {key!r}; expected one of {_KEYS}")
        actual = getattr(ctx, key)
        if not _match(matcher, actual):
            return False
    return True


def substitute(text: str, ctx: Context) -> str:
    """Replace ``{arch}`` / ``{debian_version}`` / ``{host_os}`` / ``{branch}``.

    Uses literal replacement (not str.format) so glob/braces elsewhere are safe.
    """
    out = text
    for key, val in ctx.as_subst().items():
        out = out.replace("{" + key + "}", val)
    return out
