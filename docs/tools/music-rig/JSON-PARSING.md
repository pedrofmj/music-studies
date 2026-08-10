# JSON Parsing Boundary

Status: Provisional Milestone 0 decision. Linux is proven; Windows remains an
exit gate.

## Decision

Use `json-c` 0.17 or newer as the initial control-plane JSON parser candidate.
The dependency is not locked until the same build, fixture, and packaging proof
passes on the Windows reference machine.

JSON work stays outside real-time paths. Definition loading, schema validation,
and compilation happen during startup, reload, or transaction preparation. The
result is an immutable runtime generation, and only that generation may be
published to audio or MIDI callbacks. No callback may call `json-c`, allocate
a JSON object, read a definition file, or serialize JSON.

The portable core and the current CLI skeleton remain independent of
`json-c`. The Milestone 0 target that links it is an opt-in measurement probe,
not an installed runtime.

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

## Reproduction

~~~bash
cmake -S docs/tools/music-rig -B /tmp/music-rig-build-json-c -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMUSIC_RIG_ENABLE_JSON_C_SPIKE=ON
cmake --build /tmp/music-rig-build-json-c
ctest --test-dir /tmp/music-rig-build-json-c --output-on-failure
ctest --test-dir /tmp/music-rig-build-json-c -V \
  -R '^music_rig_json_c_spike$'
~~~

The probe rejects headers or a loaded runtime older than 0.17. The option is
off by default, so a normal build neither discovers nor links `json-c`.

## Windows Gate

The selected repeatable package path is a
[vcpkg manifest](vcpkg.json) pinned to the signed `2026.04.27` registry release
commit `56bb2411609227288b70117ead2c47585ba07713`. That baseline supplies
`json-c` `0.18-20240915`. The Windows proof uses the
`x64-windows-static-md` triplet: `json-c` is static while the MSVC runtime stays
dynamic and consistent with the existing project build.

The CI reference bundle includes the probe, same fixture, unlinked baseline
CLI, static archive, and MIT license text. Nothing is installed on `beanstar`.

Before this decision becomes final, the Windows reference build must:

- obtain `json-c` 0.17 or newer through the selected repeatable package path;
- compile an equivalent Windows measurement target and parse the same fixture
  with the same validation contract;
- record executable, library, process-memory, and parse-time evidence;
- confirm static-library packaging, dynamic MSVC runtime compatibility, and
  license attribution; and
- leave the portable core and wire protocol unchanged.

Failure on Windows reopens the parser choice before product runtime code adopts
this dependency.
