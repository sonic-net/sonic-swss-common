"""Register third-party APT repositories declared via ``apt_sources:``.

Produces the shell commands to install the repo's signing key and ``.list``
file. (Only exercised by test-host setup, e.g. the Microsoft dotnet feed; the
sonic-swss-common build path declares no apt_sources.)
"""

from __future__ import annotations

from typing import List

from .model import AptSource


def register_commands(src: AptSource, use_sudo: bool = True) -> List[str]:
    # Privilege control lives here (via use_sudo, threaded from the Executor) so the
    # CLI's --no-sudo flag is honoured and runs in root/minimal containers without sudo.
    sudo = "sudo " if use_sudo else ""
    keyring = f"/usr/share/keyrings/{src.name}-archive-keyring.gpg"
    list_file = f"/etc/apt/sources.list.d/{src.name}.list"
    return [
        f"curl -fsSL {src.gpg_key_url} | gpg --dearmor | {sudo}tee {keyring} > /dev/null",
        f"curl -fsSL {src.list_url} | {sudo}tee {list_file} > /dev/null",
    ]
