# Music Rig Runtime

This directory contains the Configurable Performance Rig implementation.

Status: Milestone 0 portable core and CLI skeleton. It does not install a
service, read or write runtime state, connect to MIDI or audio, or modify the
stable Carla/PipeWire setup.

## Safety Boundary

The current single-rig deployment remains the production default. Code in this
directory uses separate names and has no installation or activation target.
Before later live experiments, verify the protected setup:

~~~bash
docs/tools/airstar-live-setup/verify-protected-baseline
docs/tools/airstar-live-setup/restore-protected-baseline
~~~

Both commands above are read-only. Restoration requires an explicit
`restore-protected-baseline --apply`.

## Build And Test

Build outside the repository working tree:

~~~bash
cmake -S docs/tools/music-rig -B /tmp/music-rig-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/music-rig-build
ctest --test-dir /tmp/music-rig-build --output-on-failure
/tmp/music-rig-build/music-rig --version
~~~

The same C sources and CMake project are intended for Linux and Windows. The
Linux `json-c` dependency proof now passes. The portability gate still needs a
Windows build runner, a selected Windows IPC transport, and the equivalent
Windows `json-c` proof.

## Atomic Generation Spike

The portable core publishes immutable mapping generations through lock-free
atomic pointers. The synthetic callback test performs 9,999 control-side
publications, verifies monotonic real-time adoption, rejects stale generation
IDs, and enforces the 20 ms control-commit ceiling. It is an isolated process
and has no audio, MIDI, graph, service, or state adapter.

## Versioned IPC Spike

The portable core encodes fixed-size protocol frames explicitly in little-endian
order. The Linux test performs 1,000 local `SOCK_SEQPACKET` request/response
round trips and enforces a 20 ms p99 ceiling. It uses an unnamed `socketpair`
and does not create a daemon, service, runtime socket file, or persistent state.

See [PROTOCOL.md](PROTOCOL.md) for the frame contract and Windows named-pipe
candidate boundary.

## Optional JSON Dependency Spike

Normal builds do not discover or link `json-c`. Enable the isolated Linux
dependency probe explicitly:

~~~bash
cmake -S docs/tools/music-rig -B /tmp/music-rig-build-json-c -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMUSIC_RIG_ENABLE_JSON_C_SPIKE=ON
cmake --build /tmp/music-rig-build-json-c
ctest --test-dir /tmp/music-rig-build-json-c --output-on-failure
ctest --test-dir /tmp/music-rig-build-json-c -V \
  -R '^music_rig_json_c_spike$'
~~~

The probe validates a compiled-runtime-shaped fixture 10,000 times and reports
parse time, peak process RSS, executable size, and linked-library size. See
[JSON-PARSING.md](JSON-PARSING.md) for the provisional dependency boundary and
[benchmarks/json-c-linux.json](benchmarks/json-c-linux.json) for the evidence.

## Protected Helper Offline Tests

Linux builds exercise the protected Arturia and SMK-25 helper sources without
installing or connecting them. The Arturia harness uses process-local JACK
mocks and temporary buffers; the SMK-25 test links a fail-fast JACK stub and
runs its built-in `--self-test` and `--check-config` modes.

No test starts a service or opens a live JACK client. See
[HELPER-OFFLINE-TESTS.md](HELPER-OFFLINE-TESTS.md) for covered behavior,
reproduction, and the remaining explicit live-hardware checks.

## Read-Only Airstar Baseline

The Milestone 0 collector observes the protected rack without changing remote
files, services, PipeWire metadata, or graph connections:

~~~bash
docs/tools/music-rig/benchmarks/capture-airstar-baseline
~~~

See [benchmarks/README.md](benchmarks/README.md) for the audited remote command
set and report behavior.
