# CodeQL and Bazel cache reuse

The ordinary [Bazel CI job](../.github/workflows/bazel.yml) caches compiled outputs
without creating a CodeQL database. The
[C++ CodeQL job](../.github/workflows/codeql-analysis.yml) rebuilds C/C++ actions so
CodeQL can observe them. In this manual build configuration, CodeQL records
compilation and linkage facts from build processes. A Bazel cache hit restores
declared build outputs without running those processes. Ordinary object, library,
and binary cache entries therefore do not populate the corresponding facts in a
new CodeQL database.

This follows from the separate roles of
[CodeQL build tracing](https://docs.github.com/en/enterprise-cloud@latest/code-security/codeql-cli/getting-started-with-the-codeql-cli/preparing-your-code-for-codeql-analysis#specifying-build-commands)
and [Bazel action caching](https://bazel.build/remote/caching#how-a-build-uses-remote-caching),
along with [CodeQL's linkage model](https://github.com/github/codeql/blob/codeql-cli/v2.27.0/cpp/ql/lib/semmle/code/cpp/Linkage.qll#L1-L16).
The local experiment below confirms this boundary for the recorded commit and tool
versions.

## Current workflow policy

The job creates a fresh Bazel output base after CodeQL initialization. This starts
a new Bazel server with the tracing environment and avoids existing local action
state. The build uses these controls:

| Control | Purpose |
| --- | --- |
| `--spawn_strategy=local` | Run build processes on the runner where CodeQL tracing is active. |
| `--nouse_action_cache` | Disable Bazel's local action cache. |
| `--disk_cache=` and `--remote_cache=` | Disable disk and remote caches of action outputs. |
| `--remote_executor=` | Disable remote execution. |
| `--noremote_accept_cached` and `--noremote_upload_local_results` | Keep the policy for remote caches explicit. |

The local action cache and disk action cache are separate mechanisms; disabling
one does not disable the other. See Bazel 8.5.1's
[local action cache control](https://github.com/bazelbuild/bazel/blob/8.5.1/src/main/java/com/google/devtools/build/lib/buildtool/ExecutionTool.java#L960-L971)
and [disk/remote cache policy](https://github.com/bazelbuild/bazel/blob/8.5.1/src/main/java/com/google/devtools/build/lib/remote/RemoteExecutionService.java#L318-L350).
Bazelisk and repository download caches remain enabled. They reuse the Bazel
executable and downloaded dependency archives, while the compiler and linker
actions still execute under CodeQL.

## Local experiment on 2026-09-27

The experiment used commit `e8ea05e8e30909fba2b7936359ee392d06ee3d49`, Bazel
8.5.1, CodeQL CLI 2.27.0, and the no-YANG build in a native AMD64 Debian Trixie
container limited to 4 CPUs and 16 GiB. It built the same six targets as the C++
CodeQL job at that commit, with `--spawn_strategy=local` and `--cxxopt=-nostdinc`.

Each case initialized a new CodeQL database, activated its tracing environment,
and then created a new Bazel output base.
Remote execution and remote caches were disabled. CodeQL's extractor cache, which
stores TRAP extraction records, was disabled. This tested ordinary Bazel action
reuse without restoring CodeQL extractor cache records. The fresh control populated a
disk cache reserved for the later cases in this experiment.

| Case | Bazel elapsed time | Disk cache hits | Local build spawns | Successful CodeQL extractions | Primary files extracted |
| --- | ---: | ---: | ---: | ---: | ---: |
| Fresh extraction control with an empty disk cache | 706.351 s | 0 | 1,138 | 935 | 552 |
| Unrestricted disk cache reuse | 95.518 s | 1,138 | 0 | 15 | 1 |
| Conservative trial reusing preparation actions | 900.850 s | 173 | 965 | 935 | 552 |

These times cover only the Bazel command on a shared host. The traced fresh control
populated the warm cache with CodeQL's build flags; the experiment did not restore
a cache from ordinary Bazel CI. Hosted cache behavior was not measured. Because
the control allowed disk cache writes, its time serves only as a fresh extraction
reference for this experiment. The conservative trial did not demonstrate a time
saving.

Each primary file is the main source file for a compilation. The fresh control
extracted 122 repository source files, the generated SWIG wrapper,
428 dependency source files, and one toolchain probe file. Its 935 extractions
consisted of 920 Bazel compiler actions plus 15 compilations of the probe.

With unrestricted disk cache reuse, all 920 compiler actions, 16 linker actions,
and 29 archive actions were cache hits. The new CodeQL database contained only the
15 toolchain probe compilations. Repository sources, the generated wrapper, and
dependency translation units were absent, despite the successful Bazel build and
zero extractor failures. The shorter build was therefore not a valid replacement
for the existing C++ analysis build.

### Conservative trial and its limit

The conservative trial kept Bazel's C/C++ compile, header, linkstamp, link, and
archive actions uncached, including dependency and tool compilations. It reused
173 preparation and generation actions while running all 920 compiler, 16 linker,
and 29 archive actions locally.

All 935 normalized compilation records matched the fresh control in paths of
compiled files, termination status, and expanded compiler arguments. The comparison
normalized paths that identify the checkout, database, output base, and compiler
temporary outputs.
All 4,176 files in the source archives had identical contents. The extracted
databases still differed:
406 file inventory rows had different declaration or function counts. A direct
query of `link_parent`, which assigns program elements to link targets, counted
657,293 rows instead of 657,046. Both databases had 18 link targets. The cause was
not established, so analysis parity remains unresolved. A full security query and
SARIF comparison was not run after these differences were found.

The current workflow retains its existing restrictions on action caches. This
trial does not justify reusing preparation action outputs yet.

## What future cache reuse must provide

Reusing compiled outputs while skipping these traced build processes would require
another mechanism to supply the associated extraction and linkage facts. If those
facts are cached, the cache keys must cover every input that affects extraction,
including sources, headers, compiler options, and tool versions. The facts must
cover repository sources, generated sources, dependencies, tools that Bazel builds
for use during the build, and the relationships created by linking. This repository
does not configure such an integration between Bazel outputs and CodeQL facts. The
pinned CodeQL Action has a
[separate extractor cache mechanism](https://github.com/github/codeql-action/blob/ec3cf9c605b848da5f1e41e8452719eb1ccfb9a6/src/trap-caching.ts#L44-L136),
but this experiment did not validate it as a replacement for compiler execution on
Bazel cache hits.

Any future policy reusing preparation outputs should establish extraction parity
and measure the time spent restoring caches and the actual eligible hits on hosted
runners before claiming a build time saving.
