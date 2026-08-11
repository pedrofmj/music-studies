# JSON Parsing Boundary

Status: Accepted Milestone 0 decision. Linux and Windows dependency proofs
pass.

## Decision

Use `json-c` 0.17 or newer as the control-plane JSON parser. The accepted
architecture decision is
[ADR 0005](../../features/0001.0000.0000.0000-configurable-performance-rig/architecture-decisions/0005-json-c-control-plane-parsing.md).

JSON work stays outside real-time paths. Definition loading, schema validation,
and compilation happen during startup, reload, or transaction preparation. The
result is an immutable runtime generation, and only that generation may be
published to audio or MIDI callbacks. No callback may call `json-c`, allocate
a JSON object, read a definition file, or serialize JSON.

The portable core, CLI, and default daemon remain independent of `json-c`. The
opt-in build contains the Milestone 0 footprint probe, the Milestone 3
compiled-definition metadata decoder, and an explicit read-only daemon command
that validates one named document and exits. None is installed, started by
default, or connected to runtime output.

## Linux Evidence

Captured on 2026-08-10 on the `centralstar` x86_64 Linux reference host:

- `json-c` 0.17 from Ubuntu packages `libjson-c5` and `libjson-c-dev`;
- 76,168-byte shared library and 166,386-byte static archive;
- 17,384-byte GCC probe and 17,520-byte Clang probe;
- 10,000 successful parse-and-validation iterations of a 1,206-byte
  compiled-runtime fixture;
- GCC average parse time of 10,858 ns and peak process RSS of 2,748,416 bytes;
- Clang average parse time of 15,268 ns and peak process RSS of 2,813,952 bytes;
  and
- the default `music-rig` binary links only to libc, while the opt-in probe
  links to `libjson-c.so.5`.

Peak RSS is the whole short-lived probe process, not incremental parser memory.
These timings are dependency evidence, not switch-latency results.

The raw evidence is in
[benchmarks/json-c-linux.json](benchmarks/json-c-linux.json).

## Linux Reproduction

~~~bash
cmake -S docs/tools/music-rig -B /tmp/music-rig-build-json-c -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMUSIC_RIG_ENABLE_JSON_C_SPIKE=ON
cmake --build /tmp/music-rig-build-json-c
ctest --test-dir /tmp/music-rig-build-json-c --output-on-failure
ctest --test-dir /tmp/music-rig-build-json-c -V \
  -R '^music_rig_json_c_spike$'
~~~

The probe and definition decoder reject headers or a loaded runtime older than
0.17. The option is off by default, so a normal build neither discovers nor
links `json-c`.

## Windows Evidence

The selected repeatable package path is a
[vcpkg manifest](vcpkg.json) pinned to the signed `2026.04.27` registry release
commit `56bb2411609227288b70117ead2c47585ba07713`. That baseline supplies
`json-c` `0.18-20240915`. The Windows proof uses the
`x64-windows-static-md` triplet: `json-c` is static while the MSVC runtime stays
dynamic and consistent with the existing project build.

The pinned build and physical proof passed on 2026-08-10:

- [workflow run 31414189191](https://github.com/pedrofmj/music-studies/actions/runs/31414189191)
  passed 10/10 default tests and 11/11 JSON-enabled tests with MSVC;
- the same 1,206-byte fixture used on Linux matched exactly on Windows;
- 10,000 parse-and-validation iterations on `beanstar` averaged 16,051 ns with
  zero failures;
- the complete probe process peaked at 5,500,928 bytes of working set;
- the 36,352-byte probe was 26,112 bytes larger than the 10,240-byte unlinked
  CLI built in the same configuration;
- the bundle contained the 649,278-byte static archive and its 2,205-byte MIT
  license file, and no `json-c` runtime DLL was imported; and
- local and `beanstar` hashes matched before execution. No dependency, service,
  repository, audio, or MIDI component was installed or opened, and the
  temporary bundle was removed.

Peak working set is the whole short-lived probe process, not incremental parser
memory. These timings remain dependency evidence, not switch-latency results.
The raw evidence is in
[benchmarks/json-c-windows.json](benchmarks/json-c-windows.json).

## Windows Reproduction

From a Visual Studio developer PowerShell with vcpkg available:

```powershell
cmake -S docs/tools/music-rig -B build/music-rig-json-c `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md `
  -DMUSIC_RIG_ENABLE_JSON_C_SPIKE=ON
cmake --build build/music-rig-json-c --config Release
ctest --test-dir build/music-rig-json-c -C Release --output-on-failure
```

The probe, metadata decoder, offline daemon command, and dependency remain
opt-in. A version or package-path change must repeat the build, exact-fixture,
linkage, license, and footprint checks.
