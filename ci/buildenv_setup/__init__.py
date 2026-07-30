"""buildenv_setup: shared build-environment setup for the SONiC dataplane repos.

This package is the single source of truth for setting up the build/test
environment of sonic-swss-common, sonic-sairedis and sonic-swss. It reads the
declarative ``build-env/`` configuration of a repo, resolves the (possibly
cascaded) set of apt/pip packages and upstream-artifact DEBs/wheels, installs
them, and runs any post-install configuration.

It is invoked as ``python3 -m buildenv_setup`` (see :mod:`buildenv_setup.cli`).
"""

__version__ = "0.1.0"

__all__ = ["__version__"]
