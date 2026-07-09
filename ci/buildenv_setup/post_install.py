"""Select and resolve ``post_install:`` entries.

Given the ordered list of :class:`~buildenv_setup.model.PostInstall` entries
(cascaded upstream entries first, then the repo's own base/tooling), apply the
current ``--scope`` and ``when:`` filters, dedup by ``name`` (first wins, i.e.
upstream cascade takes precedence), and resolve each entry's shell body.

``source:`` paths resolve against the owning bundle's ``build-env/`` directory
(recorded on the entry at load time), so a script always travels with the YAML
that declares it.
"""

from __future__ import annotations

import os
from typing import List

from .model import Context, PostInstall
from .predicates import evaluate


class PostInstallError(RuntimeError):
    pass


def select(entries: List[PostInstall], ctx: Context) -> List[PostInstall]:
    seen = set()
    chosen: List[PostInstall] = []
    for entry in entries:
        if entry.name in seen:
            continue
        if ctx.scope not in entry.scopes:
            continue
        if not evaluate(entry.when, ctx):
            continue
        seen.add(entry.name)
        chosen.append(entry)
    return chosen


def resolve_script(entry: PostInstall) -> str:
    if entry.script is not None:
        return entry.script
    if not entry.owner_build_env:
        raise PostInstallError(
            f"post_install {entry.name!r} has source: but no owning build-env dir"
        )
    path = os.path.join(entry.owner_build_env, entry.source)
    if not os.path.isfile(path):
        raise PostInstallError(
            f"post_install {entry.name!r}: source script not found: {path}"
        )
    with open(path, "r", encoding="utf-8") as fh:
        return fh.read()
