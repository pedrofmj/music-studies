# Airstar Baseline Benchmarks

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
