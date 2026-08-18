"""apt_sources.register_commands -- privilege control honours use_sudo."""
from buildenv_setup.apt_sources import register_commands
from buildenv_setup.model import AptSource

SRC = AptSource(name="dotnet", list_url="https://x/prod.list",
                gpg_key_url="https://x/microsoft.asc")


def test_register_commands_with_sudo():
    cmds = register_commands(SRC, use_sudo=True)
    assert len(cmds) == 2
    assert "sudo tee /usr/share/keyrings/dotnet-archive-keyring.gpg" in cmds[0]
    assert "sudo tee /etc/apt/sources.list.d/dotnet.list" in cmds[1]
    assert "https://x/microsoft.asc" in cmds[0]
    assert "https://x/prod.list" in cmds[1]


def test_register_commands_no_sudo():
    cmds = register_commands(SRC, use_sudo=False)
    assert "sudo" not in cmds[0]
    assert "sudo" not in cmds[1]
    assert "| tee /usr/share/keyrings/dotnet-archive-keyring.gpg" in cmds[0]


def test_register_commands_defaults_to_sudo():
    cmds = register_commands(SRC)
    assert "sudo tee" in cmds[0]
