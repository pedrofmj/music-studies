# Music Rig Benchmarks

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

[windows-backend-capabilities.json](windows-backend-capabilities.json) records
the later read-only MIDI service and compatibility mapping, ASIO registration,
Carla/package, Task Scheduler, and upstream candidate snapshot used by the
Windows adapter decision. It enumerated or opened no media endpoint and made no
machine change.

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
