"""Command-line interface for ``python3 -m buildenv_setup``.

All configuration flows via CLI args (design doc §2.2/§2.4): explicit,
self-documenting, and easy to log/audit.
"""

from __future__ import annotations

import argparse
import logging
import sys
from typing import List, Optional

from . import __version__
from .installer import Executor
from .model import Context
from . import planner


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="buildenv_setup",
        description="Set up the build/test environment for a SONiC dataplane repo "
                    "from its declarative build-env/ configuration.",
    )
    p.add_argument("--repo-dir", required=True,
                   help="path to the repo being set up (contains build-env/)")
    p.add_argument("--scope", choices=["build", "test"], default="build",
                   help="which environment to set up (default: build)")
    p.add_argument("--arch", default="amd64",
                   help="target architecture (amd64/armhf/arm64); default amd64")
    p.add_argument("--debian-version", default="bookworm",
                   help="debian/ubuntu codename (bullseye/bookworm/trixie/...)")
    p.add_argument("--host-os", default=None,
                   help="host OS identifier for host_os predicates "
                        "(default: <debian-version>-container)")
    p.add_argument("--branch", default="master",
                   help="build branch, used to resolve upstream artifacts")

    p.add_argument("--upstream-staged-dir", default=None,
                   help="directory of same-run staged upstream bundles "
                        "(<dir>/<upstream-name>/); overrides download for those upstreams")
    p.add_argument("--required-staged-upstream", action="append", default=[],
                   metavar="NAME",
                   help="fail if NAME is not found/used under --upstream-staged-dir "
                        "(repeatable)")
    p.add_argument("--org-url", default=None,
                   help="Azure DevOps org URL (default: $SYSTEM_COLLECTIONURI)")

    p.add_argument("--dry-run", action="store_true",
                   help="print the resolved plan without downloading or installing")
    p.add_argument("--no-sudo", action="store_true",
                   help="do not prefix commands with sudo (e.g. when already root)")
    p.add_argument("-v", "--verbose", action="store_true", help="debug logging")
    p.add_argument("--version", action="version", version=f"buildenv_setup {__version__}")
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s %(name)s: %(message)s",
    )

    host_os = args.host_os or f"{args.debian_version}-container"
    ctx = Context(
        arch=args.arch,
        debian_version=args.debian_version,
        host_os=host_os,
        scope=args.scope,
        branch=args.branch,
    )
    executor = Executor(dry_run=args.dry_run, use_sudo=not args.no_sudo)

    try:
        planner.run(
            ctx,
            repo_dir=args.repo_dir,
            executor=executor,
            staged_dir=args.upstream_staged_dir,
            required_staged=set(args.required_staged_upstream),
            org_url=args.org_url,
        )
    except Exception as exc:  # surface a clean error, non-zero exit
        logging.getLogger("buildenv_setup").error("%s", exc)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
