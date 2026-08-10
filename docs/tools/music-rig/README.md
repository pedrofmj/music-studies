# Music Rig Runtime

This directory contains the Configurable Performance Rig implementation.

Status: Milestone 0 technical gates complete with a portable core and CLI
skeleton. It does not install a service, read or write runtime state, connect
to MIDI or audio, or modify the stable Carla/PipeWire setup.

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
Linux and Windows `json-c` dependency proofs and hosted Windows workflow pass.
`beanstar` is the selected
[physical Windows reference machine](WINDOWS-REFERENCE.md), where the native
generation, named-pipe IPC, JSON footprint, and native process-resource proofs
pass. The
[Windows adapter baseline](../../features/0001.0000.0000.0000-configurable-performance-rig/architecture-decisions/0006-windows-platform-adapter-baseline.md)
is selected without installing or activating it.

The [Windows build contract](WINDOWS-BUILD.md) defines the hosted MSVC proof,
native reproduction commands, and the boundary between CI portability and a
physical audio reference machine.

The [physical Windows resource result](benchmarks/windows-resource-beanstar.json)
samples the short CLI process and a 60-second synthetic event-waiting daemon,
including working set, CPU, threads, handles, shutdown, and cleanup. It opens
no audio or MIDI API and does not certify the future production daemon or live
Windows backends.

The portable-core guard rejects platform headers and backend names without
case-sensitive gaps. Its self-test proves that PipeWire and Carla references
fail while neutral portable C passes. Backend adapters remain outside the core
library.

## Atomic Generation Spike

The portable core publishes immutable mapping generations through lock-free
atomic pointers. The synthetic callback test performs 9,999 control-side
publications, verifies monotonic real-time adoption, rejects stale generation
IDs, and enforces the 20 ms control-commit ceiling. It is an isolated process
and has no audio, MIDI, graph, service, or state adapter.

## Switch Benchmark Contract

The versioned [switch benchmark contract](benchmarks/SWITCH-BENCHMARK-CONTRACT.md)
defines identical Linux and Windows result fields for commit, real-time
adoption, resources, and audio-stability counters. CTest validates the contract,
a complete synthetic campaign, threshold-boundary failures, and thirteen
negative semantic cases using only the Python standard library. The fixture is
not performance evidence and no test connects to the live rack.

## Versioned IPC Spike

The portable core encodes fixed-size protocol frames explicitly in little-endian
order. The Linux test performs 1,000 local `SOCK_SEQPACKET` request/response
round trips and enforces a 20 ms p99 ceiling. It uses an unnamed `socketpair`
and does not create a daemon, service, runtime socket file, or persistent state.

See [PROTOCOL.md](PROTOCOL.md) for the frame contract and selected Windows
named-pipe boundary.

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
parse time, peak process memory, executable size, and linked-library footprint.
Linux uses the installed shared library; the opt-in Windows CI build uses the
pinned [vcpkg manifest](vcpkg.json) and a static `json-c` library. See
[JSON-PARSING.md](JSON-PARSING.md) for the accepted dependency boundary and
[benchmarks/json-c-linux.json](benchmarks/json-c-linux.json) and
[benchmarks/json-c-windows.json](benchmarks/json-c-windows.json) for the raw
platform evidence.

## Protected Helper Offline Tests

Linux builds exercise the protected Arturia and SMK-25 helper sources without
installing or connecting them. The Arturia harness uses process-local JACK
mocks and temporary buffers; the SMK-25 test links a fail-fast JACK stub and
runs its built-in `--self-test` and `--check-config` modes.

No test starts a service or opens a live JACK client. See
[HELPER-OFFLINE-TESTS.md](HELPER-OFFLINE-TESTS.md) for covered behavior,
reproduction, and the remaining explicit live-hardware checks.

## Current-Rack Startup

[CURRENT-RACK-STARTUP.md](CURRENT-RACK-STARTUP.md) is a deterministic,
reusable startup and validation transcript for the protected rack. Its commands,
safety classifications, expected counts, service list, hardware list, launcher,
and recovery policy come from
[current-rack-startup.json](current-rack-startup.json).

Check or regenerate the planned document without executing any transcript step:

~~~bash
docs/tools/music-rig/generate-current-rack-startup-transcript --check-document \
  docs/tools/music-rig/CURRENT-RACK-STARTUP.md
~~~

Carla launch and musical acceptance remain explicit operator-only phases. The
passing 2026-08-10 execution is retained as
[JSON](benchmarks/current-rack-startup-2026-08-10.json) and
[Markdown](benchmarks/current-rack-startup-2026-08-10.md) evidence.

## Read-Only Airstar Baseline

The Milestone 0 collector observes the protected rack without changing remote
files, services, PipeWire metadata, or graph connections:

~~~bash
docs/tools/music-rig/benchmarks/capture-airstar-baseline
~~~

See [benchmarks/README.md](benchmarks/README.md) for the audited remote command
set and report behavior.
