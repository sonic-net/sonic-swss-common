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
from typing import List, Optional

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


def resolve_script(entry: PostInstall, search_dirs: Optional[List[str]] = None) -> str:
    if entry.script is not None:
        return entry.script
    if not entry.source:
        raise PostInstallError(
            f"post_install {entry.name!r} has neither script nor source"
        )
    # Candidate locations, in order: the entry's own owning build-env first (a repo's
    # local script always wins), then any cascaded upstream build-env dirs (by
    # basename). The fallback lets a consumer reuse a script provided by an upstream
    # bundle just by referencing its filename, so a shared hook (e.g.
    # configure-redis-for-tests.sh) lives in exactly ONE repo -- the cascade root --
    # and downstream repos don't duplicate its body.
    candidates: List[str] = []
    if entry.owner_build_env:
        candidates.append(os.path.join(entry.owner_build_env, entry.source))
    for d in search_dirs or []:
        candidates.append(os.path.join(d, os.path.basename(entry.source)))
    for path in candidates:
        if os.path.isfile(path):
            with open(path, "r", encoding="utf-8") as fh:
                return fh.read()
    raise PostInstallError(
        f"post_install {entry.name!r}: source script {entry.source!r} not found "
        f"(looked in: {', '.join(candidates) if candidates else '(no candidates)'})"
    )
