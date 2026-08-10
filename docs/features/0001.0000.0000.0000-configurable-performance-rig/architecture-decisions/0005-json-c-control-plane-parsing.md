# ADR 0005: JSON-C For Control-Plane Parsing

Status: Accepted on 2026-08-10.

## Context

Rig and device definitions need a maintained JSON parser on Linux and Windows.
Parsing is not part of MIDI or audio callback work: definitions are loaded,
validated, and compiled before an immutable runtime generation is published.

The dependency must be repeatable, portable, small enough for a resident
control plane, and absent from the stable default CLI while the runtime is
still experimental.

## Decision

Use `json-c` 0.17 or newer for control-plane JSON parsing.

- Linux may use the distribution `json-c` shared library.
- Windows uses the pinned [vcpkg manifest](../../../tools/music-rig/vcpkg.json)
  with the `x64-windows-static-md` triplet. `json-c` is linked statically and
  the MSVC runtime remains dynamic.
- Definition loading, schema validation, and compilation happen only during
  startup, reload, or transaction preparation.
- Audio and MIDI callbacks may consume only immutable compiled generations.
  They must not parse or serialize JSON, allocate JSON objects, or read
  definition files.
- The default portable core and CLI remain independent of `json-c`. The
  dependency stays behind the opt-in `MUSIC_RIG_ENABLE_JSON_C_SPIKE` build
  boundary until runtime parsing work begins.

## Evidence

The same 1,206-byte compiled-runtime fixture passed 10,000 parse-and-validation
iterations on both reference platforms.

- Linux `json-c` 0.17 averaged 10,858 ns with GCC and 15,268 ns with Clang.
- Windows `json-c` 0.18 on `beanstar` averaged 16,051 ns with MSVC, used a
  5,500,928-byte peak working set for the complete probe process, and added
  26,112 bytes to the executable relative to the unlinked CLI.
- The Windows artifact contained the 649,278-byte static archive and its MIT
  license text. Local and reference-machine hashes matched, and the executable
  imported no `json-c` DLL.
- Normal builds on both platforms continued to pass without discovering or
  linking the dependency.

Raw reports are
[json-c-linux.json](../../../tools/music-rig/benchmarks/json-c-linux.json) and
[json-c-windows.json](../../../tools/music-rig/benchmarks/json-c-windows.json).
The operational boundary and reproduction commands are in
[JSON-PARSING.md](../../../tools/music-rig/JSON-PARSING.md).

## Consequences

The compiler can use one parser API on Linux and Windows without bringing file
I/O, JSON allocation, or parser state into real-time paths. Windows packaging
does not require a separate `json-c` runtime DLL.

The two platforms intentionally use different proven package versions within
the accepted compatibility floor. Schema and compiler tests, rather than
parser-specific behavior, remain the portable contract. A future version or
package-path change must rerun the equivalent build, fixture, license, linkage,
and footprint checks.
