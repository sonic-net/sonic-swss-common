"""Low-level command execution: apt / pip / dpkg / shell primitives.

All mutating actions go through :class:`Executor`, which honours ``--dry-run``
(log the command, don't run it) and prefixes privileged commands with ``sudo``.
"""

from __future__ import annotations

import logging
import os
import subprocess
from typing import Dict, List, Optional

log = logging.getLogger(__name__)


class Executor:
    def __init__(self, dry_run: bool = False, use_sudo: bool = True):
        self.dry_run = dry_run
        self.sudo: List[str] = ["sudo"] if use_sudo else []

    # -- primitives -------------------------------------------------------- #
    def run(self, argv: List[str], env: Optional[Dict[str, str]] = None) -> None:
        log.info("+ %s", " ".join(argv))
        if self.dry_run:
            return
        full_env = {**os.environ, **(env or {})}
        subprocess.run(argv, check=True, env=full_env)

    def run_script(self, script: str, env: Optional[Dict[str, str]] = None) -> None:
        log.info("+ bash -e -c <<'EOF'\n%s\nEOF", script.rstrip())
        if self.dry_run:
            return
        full_env = {**os.environ, **(env or {})}
        subprocess.run(["bash", "-e", "-c", script], check=True, env=full_env)

    # -- package managers -------------------------------------------------- #
    def apt_update(self) -> None:
        self.run(self.sudo + ["apt-get", "update"])

    def apt_install(self, packages: List[str]) -> None:
        if not packages:
            return
        self.run(self.sudo + ["apt-get", "install", "-y"] + packages)

    def pip_install(self, spec: str, pip_args: Optional[List[str]] = None) -> None:
        self.run(self.sudo + ["pip3", "install"] + (pip_args or []) + [spec])

    def dpkg_install(
        self,
        files: List[str],
        dpkg_args: Optional[List[str]] = None,
        apt_fix_broken: bool = False,
        env: Optional[Dict[str, str]] = None,
    ) -> None:
        if not files:
            return
        prefix = list(self.sudo)
        if env:
            prefix += ["env"] + [f"{k}={v}" for k, v in env.items()]
        argv = prefix + ["dpkg", "-i"] + (dpkg_args or []) + files
        if apt_fix_broken and not self.dry_run:
            try:
                subprocess.run(argv, check=True, env={**os.environ})
            except subprocess.CalledProcessError:
                log.warning("dpkg -i failed; running apt-get install -f to fix deps")
                self.run(self.sudo + ["apt-get", "install", "-y", "-f"])
        else:
            self.run(argv)
