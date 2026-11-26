# Lua scripts in a declared runtime

When `SWSS_COMMON_LUA_DIR` is unset or empty, Common appends the caller's path
to `/usr/share/swss/`, so relative paths such as `subdir/script.lua` work as before.
A nonempty override selects an absolute directory containing the same
owner-provided scripts. The directory is trusted process
configuration, like the runtime library root, and must not be set from network
requests. `secure_getenv` ignores it across a privileged execution boundary.
With the override, script names must be single file names. The loader
canonicalizes the directory and file, and a script symlink cannot escape that directory. A
missing script still raises an error and never falls back silently to a
different directory. These checks use POSIX/C++11 interfaces, preserving legacy
Make consumers without adding a C++17 filesystem requirement.

For example, gNMI's `sonic_data_client` ZMQ reconnect test creates a real
`ZmqProducerStateTable`. Its constructor loads
`producer_state_table_apply_view.lua`. A Bazel fixture can extract Common's
existing `//dist:libswsscommon_pkg` TAR below a private root and set
`SWSS_COMMON_LUA_DIR=<private-root>/usr/share/swss`, preserving that real Redis
and ZMQ path without installing files into the host filesystem. Installed
containers continue to consume the same TAR and require no override.

`//tests:redis_lua_path_test` reads the actual owner Lua source through
`loadLuaScript`, checks missing-file rejection, relative-root rejection, path
traversal and symlink escape, and verifies the unchanged unset/empty defaults.
The fixture comes from upstream Bazel C++ Runfiles using a constant owner label,
not an arbitrary command-line path. It uses Common's declared native runtime helper and runs
in both existing native AMD64/ARM64 YANG configurations. The dependent gNMI
suite separately proves the complete Redis/ZMQ behavior.
