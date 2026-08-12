# Music Rig Benchmarks

The [2026-08-11 Airstar Hardware Preset capture](hardware-preset-airstar-2026-08-11.json)
records the exact current SMC-PAD and SMC-PAD Pocket pad assignments, the
Pocket's non-MIDI hardware controls, and matching pre/post subscription
fingerprints. The temporary observers were removed and the operator confirmed
normal drum audio after cleanup.

## Milestone 3 Shadow Closure

[M3-SHADOW-EVIDENCE.md](M3-SHADOW-EVIDENCE.md) explains the consolidated
portable protocol, state, parity, physical resource, and approved live-shadow
proof. Its machine-readable manifest binds all raw records and the compiled
definition by SHA-256. The validator and its mutation self-test run through
CTest on Linux and Windows:

~~~bash
docs/tools/music-rig/benchmarks/validate-m3-shadow-evidence \
  --validate docs/tools/music-rig/benchmarks/m3-shadow-evidence-2026-08-12.json
docs/tools/music-rig/benchmarks/validate-m3-shadow-evidence --self-test
~~~

## Airstar Baseline

The baseline collector observes the protected Airstar setup without changing
services, files, audio links, MIDI links, metadata, or application state.

The remote observer uses only fixed read operations:

- system and software version queries;
- `pw-dump`;
- `pw-metadata -n settings`;
- one `pw-top -b -n 1` sample;
- `systemctl --user show`;
- `ps`; and
- project file hashing and XML parsing.

It returns JSON on standard output and creates no remote file. The local
collector verifies the protected artifact manifest before SSH, compares the
observation with the known-good structure, and writes JSON and Markdown
reports.

Run from the repository root:

~~~bash
docs/tools/music-rig/benchmarks/capture-airstar-baseline
~~~

To inspect the exact remote code without running it:

~~~bash
docs/tools/music-rig/benchmarks/capture-airstar-baseline --print-observer
~~~

An incomplete report is still written for diagnosis, but the command returns a
failure until the project checksum, plugin and connection counts, graph counts,
services, quantum, and sample-rate checks pass.

## Current Rack Startup

[current-rack-startup-2026-08-10.json](current-rack-startup-2026-08-10.json)
and
[current-rack-startup-2026-08-10.md](current-rack-startup-2026-08-10.md)
record the passing operator-controlled startup and musical-acceptance run.
The result binds the planned contract and validation inputs by SHA-256, records
all five controller checks, and confirms zero PipeWire errors, service
restarts, missing protected links, or Patchbay repairs.

## JSON-C Dependency

[json-c-linux.json](json-c-linux.json) records the Linux package, binary,
process-memory, and parse-time measurements for the opt-in dependency probe.
[json-c-windows.json](json-c-windows.json) records the pinned vcpkg package,
hosted build, static archive and license, exact cross-platform fixture, physical
`beanstar` parse and footprint measurements, hash verification, and cleanup.
Both halves of the dependency gate pass.

See [../JSON-PARSING.md](../JSON-PARSING.md) for the decision boundary and
evidence interpretation.

## Windows Reference Evidence

[windows-beanstar-m0.json](windows-beanstar-m0.json) records the selected
physical Windows machine, hosted artifact provenance and hashes, native
generation-adoption result, named-pipe golden-frame round trips, and cleanup
evidence. It is a synthetic portability and IPC result, not audio or MIDI
certification.

[windows-resource-beanstar.json](windows-resource-beanstar.json) records the
hash-verified native short-process and 60-second zero-event resource run. The
short `music-rig --version` process and synthetic event-waiting daemon report
working set, CPU, threads, handles, exit status, and cleanup. The runner opened
no audio or MIDI API. A separate post-run query confirmed that its temporary
directory and processes were absent after the run.

[windows-backend-capabilities.json](windows-backend-capabilities.json) records
the later read-only MIDI service and compatibility mapping, ASIO registration,
Carla/package, Task Scheduler, and upstream candidate snapshot used by the
Windows adapter decision. It enumerated or opened no media endpoint and made no
machine change.

Reproduce the physical resource run only with a reviewed, hash-verified hosted
bundle in a new directory below the current user's Windows TEMP directory. The
wrapper is [run-windows-resource-reference.ps1](run-windows-resource-reference.ps1).
It refuses pre-existing test processes, enforces the physical thresholds, and
removes its temporary directory in a `finally` block. This is synthetic runtime
resource evidence, not audio, MIDI, or production-daemon certification.

## Portable Switch Benchmark Contract

[SWITCH-BENCHMARK-CONTRACT.md](SWITCH-BENCHMARK-CONTRACT.md) defines the shared
Linux and Windows campaign format, exact timing boundaries, required load
scenarios, a separate 60-second zero-event resource observation, accepted
thresholds, stability counters, and raw-evidence rules. The machine-readable
authority is [switch-benchmark-contract.json](switch-benchmark-contract.json).

Validate the contract and its synthetic fixture without touching the live rig:

~~~bash
docs/tools/music-rig/benchmarks/validate-switch-benchmark --check-contract
docs/tools/music-rig/benchmarks/validate-switch-benchmark --self-test
~~~
