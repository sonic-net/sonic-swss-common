#!/usr/bin/env python3
"""Choose CI cache uploads; server credentials enforce access permissions."""

import os


ERROR = "Remote cache uploads require a master branch CI build."


def upload_allowed(environ=None):
    """Allow master push/manual CI; other contexts remain read-only."""
    environ = os.environ if environ is None else environ
    return (
        environ.get("GITHUB_ACTIONS") == "true"
        and environ.get("GITHUB_REF") == "refs/heads/master"
        and environ.get("GITHUB_EVENT_NAME") in ("push", "workflow_dispatch")
    )


def require_upload_allowed(environ=None):
    """Reject cache seeding outside the allowed context without echoing inputs."""
    if not upload_allowed(environ):
        raise ValueError(ERROR)


if __name__ == "__main__":
    print(str(upload_allowed()).lower())
