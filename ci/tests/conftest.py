import os
import sys

# Make the buildenv_setup package importable regardless of how pytest is invoked.
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
