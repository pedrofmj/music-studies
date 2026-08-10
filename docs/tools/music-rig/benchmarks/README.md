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

## JSON-C Linux Dependency

[json-c-linux.json](json-c-linux.json) records the Linux package, binary,
process-memory, and parse-time measurements for the opt-in dependency probe.
The result proves only the Linux half of the portability gate; Windows remains
pending.

See [../JSON-PARSING.md](../JSON-PARSING.md) for the decision boundary and
evidence interpretation.

## Portable Switch Benchmark Contract

[SWITCH-BENCHMARK-CONTRACT.md](SWITCH-BENCHMARK-CONTRACT.md) defines the shared
Linux and Windows campaign format, exact timing boundaries, required load
scenarios, resource separation, stability counters, and raw-evidence rules.
The machine-readable authority is
[switch-benchmark-contract.json](switch-benchmark-contract.json).

Validate the contract and its synthetic fixture without touching the live rig:

~~~bash
docs/tools/music-rig/benchmarks/validate-switch-benchmark --check-contract
docs/tools/music-rig/benchmarks/validate-switch-benchmark --self-test
~~~
