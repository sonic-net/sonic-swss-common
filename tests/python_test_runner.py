import glob
import importlib
import inspect
import re

files = glob.glob("test_*.py")

p = re.compile('^test_')

for f in files:
    mod_name = re.sub(".py$", "", f)

    print " * importing module " + mod_name

    mod = importlib.import_module(mod_name)

    for member in inspect.getmembers(mod):
        if not inspect.isfunction(member[1]):
            continue

        test_name = member[0]

        method = getattr(mod, test_name)

        print "  - running test: " + mod_name + "." + test_name

        method()
