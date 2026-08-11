# Music Rig Runtime

This directory contains the Configurable Performance Rig implementation.

Status: Milestones 0 through 2 are complete. Milestone 3 is in progress with a
portable, output-suppressed runtime control loop, qualified persistent-state
contract, checked definition-metadata loader, explicit-path Linux/Windows file
adapters, and inert `music-rigd` binary. It does not select production storage
locations, install a service, create an IPC endpoint, connect to MIDI or audio,
or modify the stable Carla/PipeWire setup.

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

Install the pinned authoring-only schema validator and build outside the
repository working tree:

~~~bash
python3 -m venv /tmp/music-rig-schema-venv
/tmp/music-rig-schema-venv/bin/python -m pip install \
  -r docs/tools/music-rig/requirements-schema.txt
cmake -S docs/tools/music-rig -B /tmp/music-rig-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPython3_EXECUTABLE=/tmp/music-rig-schema-venv/bin/python
cmake --build /tmp/music-rig-build
ctest --test-dir /tmp/music-rig-build --output-on-failure
/tmp/music-rig-build/music-rig --version
/tmp/music-rig-build/music-rigd --version
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

## Portable Profile Schemas

The seven strict Draft 2020-12 schemas under
[`src/performance-rigs/pedro-performance-rig`](../../../src/performance-rigs/pedro-performance-rig/)
define the portable v1 authoring boundary. The default CTest suite validates
the schemas themselves, six positive plus fourteen negative document fixtures,
twenty-eight positive/negative catalogue scenarios, and the authored five-slot
Rig with all five current Hardware Presets, five Device Profiles, the initial
`full-live-rack` Rig Profile, its Linux Platform Binding, and one inactive
Switch Trigger document. Semantic checks enforce selector order, required
model and endpoint coverage, unique aliases and local discriminators, optional
USB evidence, unambiguous shared USB IDs, unique hardware controls and
messages, evidence status, resolved preset and profile IDs, source-control
references, mapping ownership, required-slot composition, aggregate
capabilities and readiness, pinned and shared resources, initial state
ownership, explicit-composition ownership compatibility, complete binding
capability and resource coverage, known binding targets, platform isolation,
unique management events, trigger source and operation resolution, Hardware
Preset assignments, readiness ceilings, and consumed-event mapping conflicts.
Explicit `consume: false` overlaps remain valid for later passthrough behavior.
The Python
`jsonschema` dependency is used only by authoring and tests; neither the C CLI
nor the future resident runtime loads it.
The current-catalogue test also locks all five verified Hardware Presets to the
protected structured setup and the 2026-08-11 live pad capture. It locks the
Linux binding to the same Airstar device aliases, paths, checksums, and
services. The Windows binding is a contract fixture only and does not claim
physical backend support or certification. The authored trigger catalogue has
zero triggers and therefore enables no MIDI management operation.

## Deterministic Compiler Envelope

[`compile-performance-rig.py`](compile-performance-rig.py) validates the full
authored catalogue and resolves an explicit Rig Profile and Platform Binding
into canonical JSON. It records independent SHA-256 fingerprints for every
schema and portable source, the schema set, portable source set, selected
binding, and complete compiled definition. Source paths are relative POSIX
paths, so the exact golden output is portable across working directories and
Linux/Windows path layouts.

Run a no-write compilation check:

~~~bash
/tmp/music-rig-schema-venv/bin/python \
  docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --check-only
~~~

The compiler requires either `--check-only` or an explicit `--output` path. It
refuses to overwrite authored source and has no install, activation, MIDI,
audio, Carla, Patchbay, service, or persistent-state path. It compiles 72
current MIDI mappings with a direct dispatch index, 5 stable-slot input
bindings, 71 selected target bindings, 57 consolidated ownership entries, and
an explicitly empty current graph delta. See
[COMPILER.md](COMPILER.md) for the canonical table, delta, and fingerprint
contracts.

## Temporary Carla And Patchbay Materialization

[`materialize-compiled-rig.py`](materialize-compiled-rig.py) consumes the
fingerprinted compiled `full-live-rack` definition and renders Carla and
Patchbay artifacts only below the system temporary directory. It rejects
installed, repository, home, nonempty, and symbolic-link output roots, requires
an empty unapplied graph delta, and emits a deterministic checksummed bundle.
It performs no activation or runtime mutation. See
[MATERIALIZATION.md](MATERIALIZATION.md) for the input and output contract and
an offline example.

[`verify-materialized-rig-parity.py`](verify-materialized-rig-parity.py)
validates the temporary bundle manifest and proves exact normalized inventories
for all 49 plugins, 18 Carla plugin asset references, 111 Carla project
connections, and 115 Patchbay links. Full plugin subtrees include parameter
mappings and state; home and SoundFont paths and the explicitly relocated
default sink are normalized. A portable regression also locks all protected
installer/validator checksums, read-only entry points, missing plugin, asset,
and endpoint diagnostics, relocation, and repeatable output. None of these
tests claims installed-asset or live-graph availability. See
[PARITY.md](PARITY.md) for the semantic inventory contract.

## Portable Runtime Control Loop

[`runtime/core/music_rig_runtime.c`](runtime/core/music_rig_runtime.c) provides
the Milestone 3 daemon core: fixed-storage lifecycle state, a checksummed
persistent-state frame, saturating metrics, expected-generation publication,
and ABI-versioned clock/control/storage callbacks. Its event-driven loop handles
decoded status requests, idle waits, responses, and shutdown through mock
adapters. Output-enabled mode is rejected.

[`runtime/core/music_rig_definition.c`](runtime/core/music_rig_definition.c)
loads bounded definition metadata through logical storage and decoder adapters.
The optional `json-c` adapter validates the checked-in compiled
`full-live-rack` envelope and hands its metadata to an immutable generation; it
does not yet decode executable mapping tables.

[`runtime/platform/include/music_rig/file_storage.h`](runtime/platform/include/music_rig/file_storage.h)
defines explicit caller-owned UTF-8 paths. Linux and Windows implementations
read definition/state files and atomically replace state with native APIs. They
choose no default or installed path, create no directory, and are tested only
with ephemeral build-directory state files.

[`music-rigd`](music-rigd.c) is currently an inert build target. It supports
version/help output but refuses a no-argument start. An opt-in JSON build adds an
explicit offline `validate-definition` command; it reads one named definition,
reports validated metadata, and exits without a state path or output. The daemon
has no configured definition/state path, transport, installation, service, MIDI,
audio, graph, or plugin-host path. See [RUNTIME.md](RUNTIME.md) for ownership,
lifecycle, storage, adapter, metric, failure, and next-slice contracts.

Example for the opt-in build:

~~~bash
/tmp/music-rig-build-json-c/music-rigd validate-definition \
  --definition docs/tools/music-rig/tests/fixtures/compiled-current-envelope.json \
  --expected-fingerprint \
  sha256:9be68993c164802f694f4d6359c208d9937fbfcf668781ce5a1c3bff5f30cb9e
~~~

Run the validator directly:

~~~bash
/tmp/music-rig-schema-venv/bin/python \
  docs/tools/music-rig/validate-performance-rig.py --self-test

/tmp/music-rig-schema-venv/bin/python \
  docs/tools/music-rig/validate-performance-rig.py \
  --validate-root src/performance-rigs/pedro-performance-rig \
  --authority-setup docs/tools/airstar-live-setup/setup.json \
  --authority-pad-capture \
    docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json
~~~

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
