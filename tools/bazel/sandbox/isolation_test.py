"""Prove namespace isolation and reject writes outside Bazel's writable paths."""

import errno
import json
import os


def main():
    parent = json.loads(os.environ["SONIC_SANDBOX_CONTEXT"])
    namespaces = {name: os.readlink("/proc/self/ns/" + name) for name in ("user", "mnt", "pid")}
    denied = False
    error = None
    try:
        descriptor = os.open(parent["marker"], os.O_WRONLY)
    except OSError as failure:
        error = failure.errno
        if error not in (errno.EROFS, errno.EACCES, errno.EPERM):
            raise  # A missing marker or unrelated I/O error is not proof.
        denied = True
    else:
        os.close(descriptor)  # Never modify the marker, even in the negative control.
    failures = [name + "_namespace_not_isolated" for name in namespaces
                if namespaces[name] == parent["namespaces"][name]]
    if not denied:
        failures.append("outside_write_allowed")
    print("SONIC_SANDBOX_PROBE=" + json.dumps({
        "namespaces": namespaces, "marker_write_denied": denied,
        "marker_errno": error, "failures": failures,
    }, sort_keys=True), flush=True)
    assert not failures, "sandbox isolation failed: " + ", ".join(failures)


if __name__ == "__main__":
    main()
