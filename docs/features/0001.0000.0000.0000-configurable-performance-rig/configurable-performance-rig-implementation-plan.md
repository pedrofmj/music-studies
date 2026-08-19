# Configurable Performance Rig: Implementation Plan

Language: **English**

Status: Milestones 0 through 3 complete. Milestone 4 is active at the
prepared control-only profile selection gate after the first SMC-Mixer parity
cutover and rollback. The protected single-rig deployment remains the production
default.

Related documents:

- [Feature Proposal](configurable-performance-rig.md)
- [Proposta de Funcionalidade (pt-BR)](configurable-performance-rig.pt-BR.md)
- [Implementation Tracker](configurable-performance-rig-tracker.md)

## Implementation Checkpoint: 2026-08-12

Completed Milestone 0 work:

- protected-baseline manifest, read-only verifier, preview-first restore path,
  and guarded stable-capture workflow;
- isolated portable C17 core and `music-rig --version` CLI skeleton;
- audited read-only Airstar collector with machine-readable and Markdown output;
- graph-difference diagnostics that identify unexpected and missing endpoints;
- lock-free immutable-generation publication with synthetic callback adoption;
- portable versioned frames with a measured Linux `SOCK_SEQPACKET` round trip;
- opt-in `json-c` 0.17 Linux build, parse-cost, process-RSS, and binary-footprint
  proof with no dependency in the default CLI;
- pinned `json-c` 0.18 Windows static build, license and linkage audit, hosted
  MSVC suite, and hash-verified `beanstar` parse and footprint proof;
- repeatable offline parity tests for the protected Arturia and SMK-25 helpers,
  with no service, JACK-client, graph, or production-state activation;
- a versioned current-rack startup contract, deterministic planned transcript,
  and passing
  [operator execution](../../tools/music-rig/benchmarks/current-rack-startup-2026-08-10.md)
  with authority-drift, read-only-boundary, hardware, musical, and stability
  checks;
- a portable versioned switch-benchmark result contract that separates commit,
  real-time adoption, resources, and stability across idle, normal-performance,
  and high-MIDI-load scenarios; and
- an accepted
  [PipeWire graph-control decision](architecture-decisions/0001-pipewire-graph-control-deferral.md)
  that requires an empty graph delta for every pre-Milestone-6 control-only
  switch; and
- an accepted
  [Carla control decision](architecture-decisions/0002-carla-control-and-prepared-engine-boundary.md)
  that limits early switching to certified controls on loaded engines and
  reserves plugin lifecycle, state, project, and patchbay operations for
  prepared execution; and
- accepted
  [performance thresholds](architecture-decisions/0003-performance-acceptance-thresholds.md)
  for control commit, real-time adoption, prepared commit, no-event daemon
  resources, and zero attributable audio-stability failures; and
- a pinned Windows Server 2025 hosted workflow where the portable core, CLI,
  protocol fixtures, contract validators, and native generation-adoption test
  compile and pass with MSVC; and
- an accepted
  [Windows adapter baseline](architecture-decisions/0006-windows-platform-adapter-baseline.md)
  for native paths/state, per-user lifecycle, QPC timing, WinMM MIDI 1.0,
  plugin-host-owned ASIO/WASAPI, and out-of-process Carla; and
- a hash-verified native `beanstar`
  [resource run](../../tools/music-rig/benchmarks/windows-resource-beanstar.json)
  covering the short CLI process and a 60-second zero-event synthetic daemon,
  including working set, idle CPU, threads, handles, shutdown, and cleanup.

The Airstar observation matches the protected Carla checksum, 49 plugins, 111
project connections, all protected graph links, 2048-frame quantum, 48 kHz
rate, and all four stable services. Its report is `complete`. For that approved
capture, the diagnostic link
`PD-CH-1 Gain Map:events-out -> smc-pad-gain-verify:input` was temporarily
disconnected and then restored. A post-restore read-only observation confirmed
that the exact diagnostic link was again present, every protected link remained
present, and no service changed. The diagnostic link is not part of the
protected baseline.

All Milestone 0 baseline and technical gates pass. No experimental runtime is
installed or connected to the live rig. Windows audio, MIDI, plugin-host,
lifecycle, and production-runtime certification remain explicitly deferred to
Milestone 7.

Milestone 1 schema work now includes seven strict Draft 2020-12 schemas for the
Rig, Rig Profile, Device Profile, Hardware Preset, Platform Binding, Switch
Triggers, and shared types under
[`src/performance-rigs/pedro-performance-rig`](../../../src/performance-rigs/pedro-performance-rig/).
The portable contract and authored `rig.json` cover the five stable device
slots, ordered physical selectors, semantic capabilities, ownership modes,
readiness, takeover, state, raw hardware MIDI, switch safety, and future MIDI
management operations. All five current Hardware Presets are authored and
verified. The SMC-PAD and SMC-PAD Pocket include exact live-captured pad
messages; the Pocket's silent hardware-internal controls are modeled
separately. A pinned authoring-only validator checks all schemas, six valid
and fourteen invalid fixtures, twenty-eight positive and negative catalogue
scenarios, and the concrete Rig catalogue against protected and live-capture
evidence on Linux and Windows. The initial `full-live-rack` Rig Profile now
composes all five current Device Profiles, declares their aggregate
capabilities and readiness, pins all eighteen current engines, and records
shared resources. The `airstar-current` Linux binding resolves that complete
profile against protected device aliases, Carla targets, graph routes, files,
and services. A Windows contract fixture exercises the portable boundary
without claiming physical Windows support. The resolved Switch Trigger
catalogue is intentionally empty. Validation rejects duplicate management
events, unresolved sources and operations, missing Hardware Preset assignments,
readiness violations, and consumed-event overlap with any available Device
Profile mapping while retaining explicit passthrough. No runtime or deployment
path consumes these authored documents yet.

Milestone 2 has a completed authoring-only deterministic compiler envelope. It
validates the complete catalogue, resolves the selected Rig Profile and explicit
Platform Binding, emits canonical UTF-8 JSON, and records independent schema,
portable-source, binding, and generated-definition SHA-256 fingerprints. A
thirteen-case suite also locks the five input bindings, seventy-two direct MIDI
mappings, seventy-one selected target bindings, fifty-seven consolidated
ownership entries, relative encoding and takeover resolution, and an empty
current graph delta. A nonempty control-only delta fails compilation. The
authoring-only materializer consumes that definition and can render Carla and
Patchbay output only into a new or empty system-temporary descendant. It
records a deterministic checksummed manifest and has no install, runtime, or
activation path. The read-only parity gate compares complete normalized Carla
plugin subtrees, exact Carla source-target connection pairs, and stable
Patchbay endpoint selectors and eighteen normalized Carla plugin asset
references. It enforces the protected 49-plugin, 111-connection, and 115-link
counts without treating transient backend IDs or intended path/sink relocation
as semantic drift. Thirteen materializer cases cover exact home, SoundFont,
combined, and identity relocation, repeatable bytes, CRLF portability, guarded
temporary output, and missing endpoints. Ten parity cases cover missing
plugins and assets plus parameter, connection, link, selector, manifest, and
identity drift. An eight-case portable regression verifies all twenty-five
protected checksums, legacy Python and shell validation entry points,
working-directory independence, controlled missing-asset output, and no
protected changes. The CTest validation path runs these gates beside the
compiler check without modifying the protected production installer or
validator. Local GCC and Clang pass 28/28, both optional Linux JSON builds pass
29/29, and hosted
[run 31514575263](https://github.com/pedrofmj/music-studies/actions/runs/31514575263)
passes Linux 28/28, Windows 22/22, and Windows JSON 23/23.

Milestone 3 has an allocation-free portable control dispatcher, fixed-storage
versioned state, saturating metrics, expected-generation publication, and
ABI-versioned clock/control/storage contracts. A bounded loader reads compiled
definition metadata and caller-owned immutable tables through logical storage.
The opt-in `json-c` adapter validates the current `full-live-rack` envelope
and cross-checks 5 profile/input rows, 72 mappings and every compiler dispatch
index entry, 71 targets, and 57 ownership entries. A fixed 256-entry numeric
dispatch index performs mapping lookup without allocation, locks, JSON
traversal, or string comparison. The version 1 table image has explicit
capacities and occupies 451,032 bytes in the current 64-bit local builds.

Persistent state v2 is a fixed 128-byte frame containing the active Rig
Profile as well as generation, fingerprint, mode, and integrity tag. The
decoder retains compatibility with the 64-byte v1 frame. Tests cover qualified
prepared-profile restore, missing-profile fallback, every v2 byte corruption,
and atomic-replace failure. Per-device overrides and canonical JSON
fingerprint recomputation remain incomplete.

Explicit-path file adapters now perform bounded definition/state reads and
native same-directory atomic state replacement on Linux and Windows. They use
caller-owned UTF-8 paths, create no parent or default location, own no worker
thread, and are exercised only with ephemeral build-directory state files. The
opt-in `music-rigd validate-definition` command loads one named envelope,
requires its trusted fingerprint, reports bounded metadata, tables, and storage
size, and exits with output suppressed and no state path.

The daemon still rejects a no-argument start and accepts no output-enabled mode.
Local GCC and Clang pass 35/35, both optional Linux JSON builds pass 38/38, and
hosted
[run 31536417870](https://github.com/pedrofmj/music-studies/actions/runs/31536417870)
validates the immutable tables on Linux 35/35, Windows 29/29, and Windows JSON
32/32. No production
definition/state location, IPC endpoint, device, audio, graph, plugin host,
service, or live activation path exists yet, and the protected baseline remains
30/30.

Protocol v2 now freezes all nine planned operation IDs in fixed little-endian
frames with bounded identifiers and a complete 16-profile response inventory.
An allocation-free dispatcher implements status, filtered profile listing,
active validation, and dry-run prepare/switch/reset/reload operations over the
currently published immutable table image. The portable runtime now wraps that
planner with an output-suppressed global commit transaction: it reserves
rollback capacity, publishes a monotonic immutable generation, persists the
active Rig Profile, and republishes the prior mapping if persistence fails.
The portable CLI accepts the global commit command shape, but its executable
has no configured endpoint and fails closed without sending. Device/reset
commits and output-enabled adoption remain unavailable. Ephemeral Linux
`SOCK_SEQPACKET` and Windows
current-user named-pipe tests carry 1,000 mixed requests through the real
dispatcher. Local GCC/Clang pass 38/38, both optional JSON builds pass 41/41,
eight ASan/UBSan boundary tests pass, and the protected baseline remains 30/30.
Hosted
[run 31539454955](https://github.com/pedrofmj/music-studies/actions/runs/31539454955)
passes Linux 38/38, Windows 32/32, and Windows JSON 35/35, including the
current-user Windows named-pipe mock transport under `/W4 /WX`.

Immutable generation publication now has an eight-entry retirement ring and
explicit control-thread reclamation after real-time adoption. A full ring
applies backpressure without publishing. Linux recycles four caller-owned
entries through 9,999 publications, reclaims all 9,999 retired generations,
and observes at most three retired entries. Validated compiled tables derive
fixed `device.<slot>.midi-input` and `.midi-output` identities; all ten current
Rig identities remain unchanged across profile-only generations, and slot-set
drift fails before publication. No backend port is registered. Local GCC and
Clang pass 39/39, both optional JSON builds pass 42/42, eight ASan/UBSan
boundary tests pass, and the protected baseline remains 30/30. Hosted Windows
proof is complete:
[run 31546338374](https://github.com/pedrofmj/music-studies/actions/runs/31546338374)
passes Linux 39/39, Windows 33/33, and Windows JSON 36/36, including bounded
reclamation and stable-port tests under `/W4 /WX`.

The Linux host boundary now resolves bounded XDG configuration, cache, state,
and runtime paths without filesystem I/O. A fixed-storage portable diagnostic
limiter feeds an ABI-versioned structured stderr sink for journald. The
explicit `run-shadow --output-suppressed` lifecycle waits without polling,
handles clean `SIGINT`/`SIGTERM` shutdown, and opens no definition, state,
transport, MIDI, audio, graph, or plugin-host resource. Its checked-in systemd
user unit is not installed or enabled and requires a separate per-user
`shadow-enabled` marker. Local GCC and Clang pass 44/44, both optional JSON
builds pass 47/47, 18 ASan/UBSan runtime boundaries pass, and the protected
baseline remains 30/30. Hosted
[run 31549413339](https://github.com/pedrofmj/music-studies/actions/runs/31549413339)
passes Linux 44/44, Windows 34/34, and Windows JSON 37/37.

Reusable fixed-storage Arturia and SMK-25 behavior engines now reproduce the
current relative-volume, mute, audio-gate, pad, knob, layer-latch, Play, Stop,
transport, connection-replay, passthrough, and legacy-state transitions without
linking to a backend. Their event paths contain no allocation, locks, platform
or filesystem calls, JSON traversal, or string comparison. On the current
64-bit Linux build the caller-owned states occupy 24 and 2,352 bytes, guarded by
64-byte and 4,096-byte compile-time ceilings. Portable unit/boundary tests pass
locally; Linux differential fixtures compare output and state directly against
the unchanged protected sources. Local GCC/Clang pass 47/47, both optional JSON
builds pass 50/50, 12 ASan/UBSan behavior/runtime boundaries pass, and the
protected baseline remains 30/30. Hosted
[run 31551820903](https://github.com/pedrofmj/music-studies/actions/runs/31551820903)
passes Linux 47/47, Windows 36/36, and Windows JSON 39/39 under strict
`/W4 /WX`.

The fixed-storage Device/MIDI adapter and Linux input-only JACK host complete
the Milestone 3 execution boundary. Per-slot metrics prevent an aggregate count
from hiding an unobserved controller. Hosted
[run 31622122149](https://github.com/pedrofmj/music-studies/actions/runs/31622122149)
passes the final source commit on Linux 51/51, Windows 38/38, and Windows JSON
43/43. Physical Linux and Windows definition-backed mock-input workloads each
complete a full external 60-second zero-event window with 0.000% measured CPU,
3,162,112-byte and 6,381,568-byte observed peak RSS respectively, one native
wait completion, zero activity, clean shutdown, and no media API or live route.

The explicitly approved Airstar shadow session retained five input ports and
zero output ports beside the protected rack. A 413-second practice interval
recorded 2,054 events and 1,621 mapping decisions across Arturia, SMK-25,
SMC-Mixer, and SMC-PAD while production sound remained normal. Its zero Pocket
counter was rejected; a later same-source comparison independently recorded
two Pocket inputs and two Pocket mappings with all other shadow slots at zero.
The combined gate therefore proves all five slots without treating audible
production output as shadow evidence. All temporary resources were removed,
the Carla checksum, 117 graph links, 67 MIDI links, services, and restart counts
were unchanged, and the protected post-check passed 30/30. The checksummed,
machine-validated result is
[Milestone 3 shadow evidence](../../tools/music-rig/benchmarks/M3-SHADOW-EVIDENCE.md).

Milestone 3 is complete. No runtime was installed or enabled, no output path
exists, and the protected setup remains the default. At this checkpoint the
next sequential task was the Milestone 4 SMC-Mixer independent parity gate.
That cutover and rollback are now recorded as complete in the implementation
tracker; the current slice remains uninstalled and commit-disabled.
Hosted
[closure run 31637953686](https://github.com/pedrofmj/music-studies/actions/runs/31637953686)
passes the checksummed evidence gate on Linux 53/53, Windows 40/40, and Windows
JSON 45/45.

## Objective

Implement the Performance Rig model defined by the feature proposal while
preserving the verified current setup throughout the migration.

The completed system must:

- describe the entire transferable setup as a Performance Rig;
- describe the current setup as the first Rig Profile;
- support independently selectable Device Profiles;
- switch global and device profiles through the `music-rig` CLI;
- support future MIDI-triggered switching through the same runtime operation;
- preserve the existing machine-transfer and validation workflows;
- run the same authored Rig/Profile definitions and CLI contract on multiple
  operating systems through portable core code and platform adapters;
- meet measured latency, CPU, memory, and audio-stability requirements.

The first usable delivery target is CLI switching between control-only profiles.
Profiles that add or remove large sound engines follow later after preparation,
resource budgeting, and transactional graph switching are proven.

## Planning Assumptions

This plan uses the names from the feature proposal provisionally:

- complete setup: **Performance Rig**;
- global profile: **Rig Profile**;
- per-device role: **Device Profile**;
- controller-local configuration: **Hardware Preset**;
- CLI: `music-rig`;
- resident service: `music-rigd`; and
- first Rig identifier: `pedro-performance-rig`.

A naming change before schema v1 is inexpensive. A naming change after profile
files, state, IPC, and installation paths exist requires a migration, so the
terminology review is a gate before Milestone 1 is merged.

Linux is the first live target because the verified baseline uses Ubuntu 24.04,
PipeWire/JACK, systemd user services, and Carla. Windows is the required second
supported target. The core runtime, compiler, CLI behavior, IPC messages, state,
and profile schemas must be portable from their first version; only platform
adapters and bindings may differ. macOS is a later adapter target and must not
require redesigning those contracts.

## Protected Stable Baseline

The current single-rig setup is the protected production baseline. New profile
functionality is additive and experimental until it passes its milestone exit
gate and is explicitly promoted. Ordinary development, compilation, tests, and
installation must not edit or replace the protected Carla project, Patchbay
snapshots, controller helpers, services, quantum configuration, or Hardware
Preset documentation.

The repository maintains a machine-readable protected-baseline manifest with
checksums for every artifact required to rebuild the current setup. A read-only
verification command must pass before any experiment. A separate restore
command must:

1. refuse to proceed while Carla is running;
2. verify every protected source artifact before changing the machine;
3. back up the current deployed project, graph snapshot, configuration, and
   service evidence;
4. stop and disable only the experimental runtime;
5. reinstall the protected project, helpers, services, graph snapshot, and
   quantum; and
6. finish with the existing validator and instructions for the live check.

The restore command is preview-only unless an explicit apply flag is present.
No milestone may make the experimental runtime the default startup path until
the protected restore has been rehearsed successfully and the user explicitly
approves promotion. The protected artifacts may change only as a separate,
intentional stable-rig update.

## Non-Negotiable Invariants

1. The current `full-live-rack` behavior remains installable and recoverable at
   every milestone.
2. A switch never leaves a partially active target generation.
3. No allocation, filesystem access, JSON parsing, plugin loading, graph
   discovery, logging, mutex acquisition, or blocking IPC occurs in a JACK
   process callback.
4. A device switch does not reset unrelated device slots.
5. A global switch either commits all selected profiles or changes nothing.
6. MIDI management mappings remain available independently of the active musical
   profile.
7. Cold asset or plugin loading never blocks MIDI or audio processing.
8. Generated runtime artifacts are deterministic and traceable to versioned
   source definitions.
9. Runtime state never overwrites authored profile definitions.
10. Legacy deployment remains available until the new runtime passes parity,
    soak, performance, and rollback gates on `airstar`.
11. Top-level Rig and Profile documents contain semantic capabilities, not
    platform paths, service-manager details, backend port names, or plugin IDs.
12. A platform that cannot satisfy a profile fails before activation and reports
    every unresolved capability; it never applies a silent substitute or partial
    downgrade.
13. The protected single-rig project, graph, services, and startup command remain
    the default until explicit promotion.
14. Experimental services, ports, state, and installed files use separate names
    and locations and are disabled by default.
15. No automated test may mutate the live graph. A live mutation requires an
    explicit operator action, a current backup, a passing restore preflight, and
    a milestone procedure that names every expected change.
16. A rollback path must exist and be tested before the corresponding live
    ownership cutover is attempted.

## Current Baseline

The implementation starts from these checked-in contracts:

- `docs/tools/airstar-live-setup/setup.json` owns packages, hardware roles,
  controller messages, assets, services, Carla structure, and graph checksums.
- The Carla project contains 49 plugins and 111 project connections.
- The deployment Patchbay snapshot contains 115 links.
- The current graph uses five controller slots: Arturia, SMK-25, SMC-Mixer,
  SMC-PAD, and SMC-PAD Pocket.
- `arturia-main-volume-encoder` is a C/JACK MIDI and stereo-audio adapter with
  atomic state and a click-free mute ramp.
- `smk25-pad-layers` is a C/JACK stateful MIDI router with an offline self-test.
- Python materializers relocate Carla assets and output routes.
- The Patchbay tool restores semantic PipeWire links and has an event-driven
  watch mode.
- The tested PipeWire quantum is 2048 frames. At 48 kHz, one graph period is
  42.7 ms.
- M-VAVE devices share a USB product ID, so device identity cannot rely on USB
  ID alone.

Before behavior changes, Milestone 0 records a fresh baseline from the live
reference machine.

## Architecture Decisions

### 1. Declarative Source, Compiled Runtime

Authored Rig, Rig Profile, Device Profile, Hardware Preset, and trigger files
will be JSON documents validated by versioned JSON Schemas.

A Python compiler will:

1. validate source documents;
2. resolve references and inheritance-free composition;
3. bind profiles to logical device slots;
4. detect ownership and MIDI conflicts;
5. resolve semantic targets through backend adapters;
6. calculate readiness and graph deltas;
7. emit one deterministic, immutable runtime document; and
8. fingerprint every source and generated artifact.

The resident service reads only the compiled runtime document. It does not walk
the repository or interpret partially valid authoring files during a live
switch.

Python remains outside the live event path. It is used for authoring,
validation, compilation, installation, and diagnostics.

### 2. C Runtime And CLI

The latency-sensitive runtime will be implemented as portable C17 and built with
CMake on Linux and Windows. Platform-specific headers, handles, paths, and
service APIs are prohibited in the core library. The Linux real-time adapter
will initially reuse patterns from the current JACK services.

`music-rigd` will own:

- active Rig Profile and per-device overrides;
- compiled mapping generations;
- MIDI management triggers;
- switch validation and transactions;
- qualified runtime state;
- readiness and resource state;
- coordination with JACK, PipeWire, and Carla adapters; and
- runtime status and metrics.

`music-rig` will be a small C client that communicates with `music-rigd`
through a versioned IPC transport abstraction. Linux initially uses a local
Unix `SOCK_SEQPACKET` socket; Windows uses the local IPC mechanism selected by
the Milestone 0 portability spike. The protocol and CLI behavior remain
identical. The client will not invoke a shell pipeline or rebuild a project
during a switch.

The control plane will use a maintained C JSON parser rather than an ad hoc
parser. The initial dependency choice is `json-c`; Milestone 0 must prove its
Linux and Windows builds, runtime footprint, and packaging before this
dependency is locked.

### 3. One Coordinator, Adapter Boundaries

The daemon is the authoritative coordinator. Backend-specific responsibilities
remain behind adapters:

- **IPC adapter:** local authenticated request/response transport and wakeups.
- **Clock/thread adapter:** monotonic timing, atomics, threads, and nonblocking
  control-to-real-time signaling.
- **Path/state adapter:** platform configuration, cache, state, runtime paths,
  atomic replacement, and permissions.
- **Device/MIDI adapter:** discovery, stable slot binding, input/output, and
  real-time event delivery.
- **Audio/graph adapter:** optional audio gates, graph-cycle generation adoption,
  endpoint discovery, and minimal graph deltas.
- **Plugin-host adapter:** project identity, available ports, plugin readiness,
  semantic parameters, and prepared-engine lifecycle.
- **Service adapter:** installation, startup, restart, readiness, and recovery.

The Linux implementations use Unix IPC, XDG paths, JACK/PipeWire, Carla, and
systemd. Windows implementations satisfy the same interfaces with native
Windows paths, IPC, device/audio/MIDI, plugin-host, and service mechanisms.

The first runtime can reuse logic extracted from the two existing C services.
Those services remain installed until their behavior is running inside or behind
`music-rigd` and has passed live parity.

### 4. Portable Core And Platform Adapters

The portable core owns profile/state types, compilation contracts, semantic
mapping, switch planning, transactions, error codes, IPC messages, and metrics.
It cannot include JACK, PipeWire, systemd, Unix filesystem, or Windows API
details. Platform adapters implement explicit interfaces and advertise
capabilities to the compiler and runtime.

The same source documents compile for a selected platform binding. A binding may
map a semantic target to a different compatible plugin or graph primitive, but
the compiler rejects missing capabilities and behavioral downgrades.

### 5. Stable Slots And Semantic Targets

Physical discovery binds devices to stable slots such as:

- `arturia-main`;
- `smk25-main`;
- `smc-mixer-main`;
- `smc-pad-main`; and
- `smc-pad-pocket`.

Bindings use an ordered selector set: expected model, semantic PipeWire alias,
port purpose, and an optional locally persisted discriminator. USB ID is only
supporting evidence.

Profiles address semantic targets such as `master.volume`,
`equalizer.band.63hz.gain`, or `smk-layer.1.volume`. Backend adapters resolve
those names to current MIDI CCs, JACK ports, Carla parameters, or PipeWire
links.

### 6. Immutable Generations

Every successful compilation produces an immutable runtime generation.

For a control-only switch:

1. the control thread locates the already compiled target;
2. it validates current device and resource readiness;
3. it prepares a new active-state view using preallocated storage;
4. it atomically publishes the next generation pointer; and
5. the JACK callback adopts it at the next graph cycle boundary.

Old generations remain allocated until the real-time callback has advanced past
them. Reclamation occurs on the control thread.

Graph-changing switches use the same generation model but add prepare, graph
delta, ramp/crossfade, and rollback stages before publication.

Management MIDI events do not execute a switch inside the JACK callback. The
callback writes a fixed-size request into a preallocated lock-free queue and
signals the control thread through a nonblocking wakeup primitive. The control
thread validates and executes the normal switch transaction. Queue exhaustion
drops the management request, increments a metric, and leaves the active
generation unchanged; it must never block the callback.

### 7. Readiness Classes

The runtime preserves the feature proposal's readiness classes:

- `control-only`: no new engine or graph object is needed;
- `prepared`: dependencies exist and are ready but silent or disconnected; and
- `cold`: blocking loading or installation is still required.

Normal CLI and MIDI switches accept only `control-only` or ready `prepared`
targets. A separate explicit `--allow-cold` CLI path may perform non-live
preparation. MIDI triggers never allow cold loading.

## Target Runtime Layout

The portable core with its initial Linux adapter realization is:

```text
Authored JSON + JSON Schemas
            |
            v
  compile-performance-rig.py
            |
            v
 immutable compiled runtime document
            |
            +-----------------------------+
            |                             |
            v                             v
       music-rigd <--- Unix socket ---> music-rig
            |
     +------+------+----------------+
     |             |                |
     v             v                v
 JACK adapter  PipeWire adapter  Carla adapter
     |             |                |
 MIDI/audio     graph deltas     engines/parameters
```

Windows replaces the Unix/JACK/PipeWire/Carla edges with its platform adapters;
the compiler, runtime document, core transaction engine, CLI, and protocol stay
the same. The active mapping table is read-only to the platform's real-time
callback. CLI requests and MIDI management events enter the same portable switch
transaction API.

## Files And Installed State

### Proposed Repository Files

```text
src/performance-rigs/pedro-performance-rig/
  rig.json
  schemas/
    common.schema.json
    rig.schema.json
    rig-profile.schema.json
    device-profile.schema.json
    hardware-preset.schema.json
    switch-triggers.schema.json
  rig-profiles/
    full-live-rack.json
  device-profiles/
    arturia-main/multi-instrument-rack.json
    smk25-main/ambient-pad-layers.json
    smc-mixer-main/eight-band-eq.json
    smc-pad-main/drum-set.json
    smc-pad-pocket/drum-set.json
  hardware-presets/
  platform-bindings/
    linux/
    windows/
  switch-triggers.json

docs/tools/music-rig/
  README.md
  ARCHITECTURE.md
  PROTOCOL.md
  compile-performance-rig.py
  validate-performance-rig.py
  music-rig.c
  music-rigd.c
  CMakeLists.txt
  runtime/
    core/
    platform-api/
  platform/
    linux/
    windows/
  tests/
    contract/
  benchmarks/
  installers/
    linux/
    windows/
```

Implementation may split C sources differently, but real-time, control-plane,
protocol, adapter, and state ownership must remain explicit.

### Linux Installed Paths

```text
~/.local/bin/music-rig
~/.local/bin/music-rigd
~/.config/systemd/user/music-rigd.service
~/.config/music-rig/config.json
~/.cache/music-rig/compiled/<definition-sha256>/runtime.json
~/.local/state/music-rig/active.json
~/.local/state/music-rig/device-state/<slot>/<profile>.json
$XDG_RUNTIME_DIR/music-rig/control.sock
```

Paths must honor `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, `XDG_STATE_HOME`, and
`XDG_RUNTIME_DIR` when set.

The Windows path adapter uses the corresponding per-user executable,
configuration, cache, state, and runtime locations exposed by the operating
system. Authored profiles cannot contain either path family. Status reports the
resolved paths, and cross-platform tests verify that both layouts represent the
same logical files and state keys.

Authored JSON stays in Git. Compiled cache and runtime state do not.

## IPC Contract

The protocol is versioned independently from the profile schema.

Minimum request operations:

- `status`;
- `list-profiles`;
- `prepare-global`;
- `prepare-device`;
- `switch-global`;
- `switch-device`;
- `reset-device-override`;
- `reload-compiled-definition`; and
- `validate-active`.

Every request contains a protocol version, request ID, operation, expected
current generation when relevant, and operation-specific arguments.

Every response contains:

- request ID and protocol version;
- success or stable error code;
- previous and resulting generation IDs;
- active Rig Profile and device overrides;
- readiness result;
- control-plane duration;
- effective real-time-cycle adoption timestamp when applicable;
- warnings; and
- rollback result after any failed commit.

Planned CLI exit-code families:

- `0`: success;
- `2`: invalid command or arguments;
- `3`: invalid definition or profile;
- `4`: target not ready or cold;
- `5`: ownership, state, or generation conflict;
- `6`: runtime adapter failure;
- `7`: timeout; and
- `8`: rollback failed and manual recovery is required.

Exact response fields and error codes are frozen in `PROTOCOL.md` before the
first CLI release.

## Active State Rules

The active state is:

```text
active Rig Profile
+ zero or more explicit Device Profile overrides
+ qualified parameter state
+ definition fingerprint
+ committed generation ID
```

A global switch replaces the active Rig Profile and clears all per-device
overrides. A device switch sets or replaces one override. Resetting an override
returns that slot to the active Rig Profile's selection.

State writes use temporary-file, `fsync`, and atomic rename behavior consistent
with the current C services.

On restart:

1. restore the last committed state only when its definition fingerprint and
   required profiles are still available;
2. otherwise activate `full-live-rack` in a safe muted/paused state; and
3. report the fallback reason through status and the journal.

Absolute controls declare takeover behavior. The initial default is
`ignore-until-moved` for sound-shaping parameters and `pickup` for volume
where the target value is known. No profile may silently default to an unsafe
jump for master or monitor volume.

## Switch Transaction

### Control-Only Transaction

1. Parse and authenticate the IPC or management-MIDI request.
2. Resolve the requested Rig or Device Profile.
3. Check the expected active generation.
4. Validate slot binding, ownership, and readiness.
5. Build the next active view from precompiled objects.
6. Publish the generation atomically.
7. Wait for or observe JACK adoption when the caller requests synchronous
   confirmation.
8. Persist committed active state.
9. Return timing and resulting state.

### Prepared Graph Transaction

1. Perform all control-only validation.
2. Confirm that every engine, port, and asset is prepared.
3. Calculate the minimal external and Carla graph delta.
4. Stage new links or engines while they are silent.
5. Apply a bounded mute ramp or crossfade.
6. Commit the graph delta and active generation.
7. Confirm required ports, links, and engine health.
8. Persist state and release unpinned old resources.
9. Roll back the full delta if any commit verification fails.

No generated project file is rewritten in the critical commit interval.

## Performance Measurement Contract

The current 2048-frame quantum creates a 42.7 ms JACK/PipeWire period at 48 kHz.
The implementation must report two different latency measurements:

- **control commit latency:** request received to generation pointer published;
- **effective adoption latency:** request received to the first JACK cycle using
  the new generation.

The feature proposal's 20 ms control-only target applies to control commit.
Effective adoption must be no more than one current graph period plus 5 ms after
commit. The benchmark records both; they must not be combined into a misleading
single number.

Those values describe the Linux JACK/PipeWire baseline. Other platform adapters
measure adoption at their equivalent real-time processing-cycle boundary and
use their configured period for the same one-period-plus-5-ms rule. The control
commit definition is identical on every platform.

Prepared audio switching measures request receipt through audible graph commit,
including its ramp or crossfade.

Benchmark rules:

- use the platform clock adapter's highest-resolution monotonic clock normalized
  to nanoseconds; Linux uses `CLOCK_MONOTONIC_RAW`;
- report p50, p95, p99, maximum, and failure count;
- run at least 1,000 control-only switches per test condition;
- test idle, normal performance, and intentionally high MIDI-event load;
- record platform xruns, dropouts, and deadline errors before and after the run;
- record daemon CPU time, wakeups, RSS, and peak prepared-resource memory;
- separate daemon memory from plugin-host/plugin/sample memory; and
- retain raw benchmark JSON with the machine and definition fingerprints.

Linux and Windows retain separate reference-machine thresholds but run the same
benchmark scenarios and emit the same result schema.

A 20 ms control-plane commit that takes 50 ms to reach the audio cycle is
reported as those two values, not as a 20 ms end-to-end switch.

## Milestone 0: Baseline And Technical Gates

### Tasks

- Record the current setup as an immutable protected-baseline manifest.
- Add a read-only protected-baseline verifier.
- Add a preview-by-default one-command restore path for the current setup.
- Document the explicit opt-in boundary for every future live experiment.
- Capture a fresh project, full graph, MIDI graph, deployment graph, packages,
  services, sample rate, quantum, CPU, memory, and xrun baseline from `airstar`.
- Exercise and record the existing Arturia and SMK self-tests.
- Add a repeatable current-rack startup and validation transcript.
- Define benchmark JSON output and timing boundaries.
- Select and record a Windows reference machine.
- Build a minimal CMake portable-core and CLI target on Linux and Windows.
- Prove a minimal versioned IPC round trip using `SOCK_SEQPACKET` on Linux and
  the candidate local Windows transport.
- Prove `json-c` build availability and measure its footprint on both targets.
- Add a build check that rejects platform headers and symbols from the core
  library.
- Prove an atomic mapping-pointer swap observed from a synthetic JACK callback.
- Prove the equivalent generation-adoption interface with a mock Windows
  real-time adapter.
- Record reliable Windows short-process and 60-second zero-event synthetic
  daemon resource measurements, including working set, CPU, threads, handles,
  shutdown, and cleanup.
- Decide whether native PipeWire graph control can be deferred to Milestone 6
  without blocking control-only profiles.
- Investigate the supported Carla live-control boundary for parameter changes,
  plugin readiness, and prepared engine activation.

### Deliverables

- baseline report under `docs/tools/music-rig/benchmarks/baseline-airstar.md`;
- protected-baseline manifest and checksum verification report;
- preview-by-default restore command with timestamped pre-restore backups;
- documented restore rehearsal procedure;
- machine-readable baseline JSON;
- hash-verified Windows short-process and zero-event resource evidence;
- short architecture decision records for JSON parsing, portable core
  boundaries, Linux/Windows IPC, real-time generation publication, PipeWire
  control, Windows backend selection, and plugin-host control; and
- confirmed or revised benchmark thresholds with reasons.

### Exit Gate

No schema or runtime merge proceeds until the baseline is reproducible and the
control-plane spike demonstrates that the selected C and IPC design can meet the
20 ms commit target with substantial margin. The portable core, CLI skeleton,
protocol types, and JSON dependency must compile on Linux and Windows before
Linux live adapters are allowed to shape public core interfaces. The protected
artifact verifier must pass, the restore command must pass its non-mutating
tests, and the current setup must remain the default; a production restore
rehearsal is required before the first live ownership cutover, not during this
read-only milestone.

## Milestone 1: Schemas And Current Profile Extraction

### Tasks

- Add the seven JSON Schemas and shared identifier definitions.
- Define schema version `music-studies/performance-rig/v1`.
- Define stable slots and ordered physical-discovery selectors.
- Extract current controller-local settings as Hardware Presets.
- Extract the five current Device Profiles.
- Add `full-live-rack` as the initial Rig Profile.
- Add explicit ownership for inputs, outputs, CC transforms, state keys,
  parameters, plugins, routes, and services.
- Define ownership modes for exclusive targets, shared event destinations, and
  read-only observations.
- Define required semantic capabilities separately from platform bindings.
- Add Linux bindings for the current setup and Windows binding fixtures for
  schema/contract tests.
- Reject operating-system paths and backend-specific IDs in top-level profiles.
- Represent readiness class and takeover policy in every profile.
- Preserve the current `setup.json` as the deployment authority during this
  milestone; new definitions reference it where duplication would create drift.
- Rename or cross-reference the existing controller-profile documentation as
  Hardware Presets without breaking existing links.

### Tests

- schema valid and invalid fixtures;
- duplicate ID and unresolved reference rejection;
- same-model, different-slot binding tests for SMC-PAD and Pocket;
- shared M-VAVE USB ID ambiguity tests;
- MIDI channel/type/number collision tests;
- private transformed CC collision tests;
- ownership conflict tests; and
- trigger-versus-musical-mapping conflict tests.

Cross-platform schema coverage additionally compiles the same authored profile
with Linux and Windows binding fixtures, verifies complete missing-capability
diagnostics, and rejects platform-specific data that leaks into portable
profiles.

### Exit Gate

The extracted `full-live-rack` describes every current controller role and
dependency, and the validator rejects each known conflict class.

## Milestone 2: Deterministic Compiler And Parity Materialization

### Tasks

- Implement `compile-performance-rig.py`.
- Produce canonical JSON with stable ordering and a source fingerprint.
- Accept an explicit platform binding and record its independent fingerprint.
- Produce clear unresolved-capability output before materialization.
- Compile semantic mappings into direct runtime lookup tables.
- Compile per-target ownership and graph deltas.
- Extend Carla and Patchbay materializers to accept the compiled
  `full-live-rack` definition.
- Generate the existing Carla project and deployment graph in a temporary
  directory.
- Compare semantic plugin, parameter, connection, asset, service, and route
  inventories rather than relying only on byte identity.
- Add `--check-only`, `--output`, and deterministic diagnostic output.
- Integrate compilation into the existing setup validator without changing the
  installed runtime yet.

### Tests

- compiler unit tests;
- golden compiled-runtime fixture;
- stable output across repeated runs and different working directories;
- home and SoundFont relocation tests;
- missing plugin/asset/endpoint diagnostics;
- current project 49-plugin/111-connection parity;
- deployment graph 115-link parity; and
- full existing installer/validator regression.

Portable compiler tests run from Linux and Windows path layouts, verify that the
portable source fingerprint stays identical, and verify that only the selected
platform-binding portion of compiled output differs.

### Exit Gate

The new authored definitions deterministically materialize a setup semantically
equivalent to the current checked-in deployment. No live routing has changed.

## Milestone 3: Runtime, CLI, And Shadow Mode

### Tasks

- Implement the versioned IPC protocol.
- Implement the portable `music-rigd` control loop, state store, metrics, and
  platform interfaces.
- Build the daemon and CLI on Windows with mock MIDI/audio/graph adapters and the
  selected Windows IPC/path implementations.
- Add the Linux systemd service adapter.
- Implement `music-rig status`, `status --json`, `profiles list`,
  `validate`, and dry-run switch commands.
- Load one compiled definition at daemon startup.
- Implement immutable mapping generations and safe reclamation.
- Add the JACK adapter with stable input/output ports for each device slot.
- Extract reusable mapping and state logic from the existing C services without
  changing their installed behavior.
- Run the new JACK adapter in shadow mode: receive duplicated MIDI inputs,
  calculate outputs and switches, but emit no musical or control output.
- Compare shadow decisions against current service output using captured MIDI
  event fixtures.
- Add a portable diagnostics sink with rate limiting outside the real-time
  callback; the Linux adapter writes it to the journal.

### Tests

- C unit tests for MIDI parsing, transforms, debounce, takeover, and state;
- protocol request/response and malformed-message tests;
- stale expected-generation conflict tests;
- daemon restart and definition-fingerprint fallback tests;
- client timeout and daemon-unavailable behavior;
- shadow equivalence for Arturia relative volume/mute;
- shadow equivalence for SMK layer latch, Play, Stop, and state recall;
- Linux/Windows protocol and state-format contract tests;
- Windows mock-adapter global/device switch transaction tests;
- CMake build and test execution on both platforms;
- no-allocation/no-lock callback audit; and
- idle CPU, wakeup, and RSS benchmark.

### Exit Gate

The daemon can run for a live practice session in shadow mode without xruns,
meaningful idle CPU regression, port conflicts, or output differences in
captured behavior. The same core and CLI pass protocol, state, and transaction
tests on Windows before Linux-only live behavior is allowed to change their
interfaces.

## Milestone 4: Control-Only Switching

This milestone produces the first usable profile-switching release.

### Tasks

- Route SMC-Mixer input through the daemon's stable device-slot port.
- Implement `eight-band-eq` through the compiled mapping table.
- Add the first alternative profile, `multilevel-volume`, using already loaded
  mixers and effects so no cold engine is required.
- Route SMC-PAD and SMC-PAD Pocket through independent stable slot ports.
- Preserve `drum-set` for both pad slots and add `pad-layer-controller` for
  at least one slot using engines already loaded by the current rack.
- Prove the supported mixed assignment
  `smc-pad-main=pad-layer-controller` plus
  `smc-pad-pocket=drum-set`.
- Reject a combination when two profiles claim the same exclusive layer target.
- Add a second control-only Rig Profile that differs from `full-live-rack` by
  selecting `multilevel-volume` and the mixed pad assignment. Its final name is
  chosen with the authored profile review.
- Implement `music-rig switch --device ... --profile ...`.
- Implement `music-rig switch --global ...` for Rig Profiles composed only of
  current prepared resources.
- Implement per-device overrides and reset.
- Add synchronous commit and optional JACK-adoption confirmation.
- Add profile-specific state and takeover behavior.
- Add minimal static routes needed by both profiles; only the active profile
  emits events to its owned targets.
- Move Arturia and SMK behavior behind the daemon only after independent parity
  cutovers.
- Preserve legacy service units in a disabled-but-restorable state.

### Tests

- 1,000-switch EQ/volume benchmark;
- 1,000-switch drum/layer benchmark;
- global atomicity with injected validation failure;
- unrelated slot state preservation;
- simultaneous different SMC-PAD and Pocket profiles;
- conflicting pad-layer target rejection;
- fader takeover and no-master-jump tests;
- daemon/service restart with an active override;
- disconnect and reconnect each controller;
- repeated profile switching under dense note and CC traffic;
- current full-rack live validation; and
- no new xruns or deadline errors.

### Exit Gate

Both global and device CLI commands are operational. Control commit, JACK
adoption, idle CPU, RSS, and xrun results meet the recorded Milestone 0 gates.
SMC-PAD and Pocket run different profiles concurrently. Rollback to the legacy
services is documented and tested.

## Milestone 5: MIDI Management Triggers

### Tasks

- Capture the exact Arturia pad endpoint, channel, message type, number, press,
  and release values selected for management.
- Store the controller-local assignment as a Hardware Preset.
- Add persistent management-trigger compilation.
- Process management triggers before musical mappings.
- Implement CC, note, and program-change matching.
- Implement press/release edge detection, debounce, consume/passthrough, and
  recursion prevention.
- Enqueue a fixed-size switch request from the JACK callback and execute it on
  the control thread through the internal switch API; never execute the CLI or
  block the callback.
- Support global and per-device switch actions.
- Add optional next/previous profile actions only after explicit ordering is
  stable.
- Add feedback only after inbound Arturia LED/display behavior is captured and
  verified.

### Tests

- one switch per physical press;
- release event does not retrigger;
- consumed event does not reach an instrument;
- passthrough event reaches both paths exactly once;
- management mapping survives Arturia musical-profile changes;
- cold and invalid targets are rejected without blocking the event path;
- recursive actions and conflicting mappings fail compilation;
- CLI and MIDI produce identical resulting state; and
- end-to-end MIDI-trigger latency benchmark.

### Exit Gate

At least one Arturia pad can switch a Device Profile and one can switch a Rig
Profile reliably throughout a live practice session, including switching back
from every reachable state.

## Milestone 6: Prepared Engines And Graph Deltas

### Tasks

- Implement the native PipeWire graph adapter selected in Milestone 0.
- Implement the Carla control adapter selected in Milestone 0.
- Add `prepare --global`, `prepare --device`, readiness status, pinning, and
  bounded warm-resource eviction.
- Keep plugin loading and asset hashing outside real-time processing.
- Add graph-delta staging, verification, commit, and rollback.
- Add click-free ramp or crossfade policy per audio-owning profile.
- Add `--allow-cold` only for explicit non-live CLI operation.
- Implement one Arturia sound-design profile against a verified parameterized
  plugin.
- Implement a second materially different Rig Profile to prove global prepared
  switching.
- Record engine-specific semantic parameter capabilities rather than assuming
  all organ, synth, choir, or piano plugins expose the same controls.

### Tests

- preparation success, missing asset, and plugin failure;
- memory ceiling and deterministic eviction;
- pinned resource preservation;
- graph-delta dry run;
- failure before commit changes nothing;
- failure during commit restores the previous complete graph;
- crossfade/ramp click and xrun tests;
- repeated prepared global switching;
- cold MIDI trigger rejection; and
- CPU and memory under the configured warm set.

### Exit Gate

One prepared Device Profile and one prepared Rig Profile switch meet the agreed
latency and stability targets. Forced failures prove complete rollback.

## Milestone 7: Windows Adapter And Multiplatform Certification

### Tasks

- Implement the Windows IPC, clock/thread, path/state, service, device/MIDI,
  audio/graph, and plugin-host adapters selected by the Milestone 0 decisions.
- Use the same portable C core, CLI sources, JSON Schemas, compiler, IPC
  messages, compiled document contract, and state model as Linux.
- Add Windows platform bindings that satisfy one complete Rig Profile. Prefer
  `full-live-rack`; if a platform plugin is unavailable, bind a verified
  semantic equivalent and document the substitution.
- Reject any binding that cannot provide the declared parameter range, event
  behavior, audio role, state semantics, or switch readiness.
- Add a Windows installer, service lifecycle, validator, capability report, and
  rollback operation.
- Bind the Arturia, SMC-Mixer, SMC-PAD, SMC-PAD Pocket, and SMK-25 slots without
  changing their authored Device Profiles.
- Verify global, device, and MIDI-triggered switching.
- Import portable active/profile state created on Linux, and perform the reverse
  import, excluding explicitly platform-local device-discovery evidence.
- Run the common benchmark suite and record Windows-specific thresholds and
  plugin/resource memory separately.
- Publish a platform support matrix listing complete, substituted, optional,
  and unsupported capabilities.

### Tests

- portable-core and CLI tests on both operating systems;
- protocol and state-format byte/semantic compatibility;
- clean Windows install and existing-install upgrade;
- device discovery, disconnect, reconnect, and same-model slot identity;
- complete Rig Profile materialization and validation;
- CLI global and device switching;
- Arturia MIDI management switching;
- state restart and cross-platform import;
- cold-profile rejection and prepared-profile activation;
- Windows audio dropout/deadline and switching benchmarks; and
- forced adapter failure with complete rollback.

### Exit Gate

At least one complete Rig Profile passes installation, validation, global
switching, device switching, MIDI management switching, state restore,
performance measurement, failure recovery, and rollback on both Linux and
Windows from the same authored musical definitions. Platform differences are
contained in bindings and adapters and are visible in the capability report.

## Milestone 8: Deployment Cutover And Cleanup

### Tasks

- Integrate Linux Rig compilation, runtime installation, service installation,
  and validation into `install-airstar-live-setup`.
- Extend `validate-airstar-live-setup` with compiled definition, daemon,
  profile, IPC, state, and live graph checks.
- Integrate the same compiler and compiled definitions into the Windows
  installer, service manager, validator, and rollback command.
- Extend capture tooling so intentional current-profile changes update source
  profiles and fingerprints.
- Add backup and restore of authored Hardware Presets where tooling permits.
- Provide a one-command rollback to the preserved legacy project, graph, and
  services.
- Perform clean-machine and existing-machine upgrades on the Linux and Windows
  reference machines.
- Run sustained live soaks with switching and controller reconnects on both
  platforms.
- Update operating procedures, current state, tool index, platform support
  matrix, and recovery docs.
- Remove duplicated legacy runtime code only after the rollback window closes.

### Exit Gate

A clean compatible Linux machine and a clean compatible Windows machine can
install `pedro-performance-rig`, activate the certified complete Rig Profile,
switch its supported profiles, validate the live state, and roll back without
manual graph editing. Both use the same authored profile source and CLI
contract.

## Test Strategy

### Static And Schema Tests

Validate every authored document, reference, selector, ownership claim,
semantic target, trigger, readiness declaration, and state policy.

### Compiler Tests

Use canonical golden files and semantic parity inventories. Generated output
must be deterministic across repeated runs.

### Cross-Platform Contract Tests

- Build the portable core and CLI from the same sources on Linux and Windows.
- Compile the same authored Rig and Device Profiles with platform-specific
  binding fixtures.
- Assert identical IPC message semantics, CLI commands, error codes, schema
  versions, compiled core data, and portable state fields.
- Import portable state in both directions and exclude only documented
  platform-local discovery evidence.
- Assert that missing or incompatible semantic capabilities fail validation
  before activation instead of degrading silently.

### Runtime Unit Tests

Keep event transformation independent from backend audio buffers so captured
MIDI fixtures can exercise mapping, latch, debounce, trigger, takeover, and
state logic offline, following the current SMK self-test pattern.

### Integration Tests

Start the daemon with temporary platform config, cache, state, and runtime
locations plus a test IPC endpoint. Exercise CLI requests, concurrent requests,
stale generations, restart recovery, corrupted state, adapter failures, and
rollback on both operating systems.

### Virtual Audio/MIDI Tests

Use PipeWire/JACK virtual ports on Linux and equivalent Windows virtual or
loopback endpoints to validate stable logical ports, routing, graph adoption,
and reconnect behavior without physical controllers.

### Live Hardware Tests

Use all five controller slots on the Linux and Windows reference machines.
Verify exact endpoint, channel, message type, number, and values before
accepting a Hardware Preset.

### Performance Tests

Add one entry point:

```bash
docs/tools/music-rig/benchmarks/run-switch-benchmark \
  --iterations 1000 \
  --rig pedro-performance-rig \
  --device smc-mixer-main \
  --profiles eight-band-eq,multilevel-volume
```

The same benchmark contract runs through a platform launcher on both operating
systems and stores raw JSON plus a concise Markdown summary. Results use common
metric names and explicit platform thresholds; they are not reviewed only by
perception.

## Deployment Strategy

Every stage is opt-in. The protected single-rig startup remains unchanged, and
no general installer enables an experimental service or replaces a stable
artifact. Before Stage C or any later ownership change, the restore preflight,
backup, and rehearsal gate must already have passed.

### Stage A: Definitions Only

Install no new service. Compile and compare the current profile with the legacy
deployment.

### Stage B: Shadow Runtime

Run `music-rigd` with duplicated MIDI inputs and no outputs. Preserve all
legacy services and routes.

### Stage C: One-Device Cutover

Move only SMC-Mixer ownership to the new runtime. Keep a separate legacy
Patchbay snapshot and service restore command.

### Stage D: Remaining Control Cutover

Move Arturia and SMK behavior after device-specific parity. Add independent
SMC-PAD and Pocket routing.

### Stage E: MIDI Management

Enable only validated, reversible trigger mappings. Reserve a physical control
that always returns to `full-live-rack`.

### Stage F: Prepared Graph Switching

Enable prepared engine and graph changes only after resource and rollback gates
pass.

### Stage G: Windows Certification

Repeat the applicable shadow, device, management-trigger, prepared-switch, and
rollback stages with the Windows adapters. Mark Windows as supported only after
the complete-profile gate in Milestone 7 passes.

## Rollback Strategy

Before every platform cutover:

- retain the previous platform binding, runtime binaries, service registration,
  configuration, portable state, and platform-local discovery state;
- capture the active Rig generation and compiled-definition fingerprints;
- retain the previous plugin-host project and live graph snapshot;
- record the platform-specific plugin, route, service, and audio deadline
  baseline; and
- generate a rollback manifest and preflight report.

Linux additionally retains the current Carla project checksum, 115-link
deployment snapshot, C binaries, configuration, state, and systemd units.
Windows retains the previous installer version, service registration,
plugin-host project, device bindings, and certified graph snapshot.

Rollback must:

1. stop and disable the platform `music-rigd` service;
2. restore the previous runtime, platform binding, plugin-host project, graph,
   service registration, configuration, and state;
3. restore and start the previous Arturia and SMK routing services where they
   existed;
4. validate the recorded platform baseline;
5. on Linux, additionally validate 49 plugins, 111 project connections, 115
   deployment links, four expected services, and the 2048-frame quantum; and
6. leave new authored profiles and diagnostic data intact for investigation.

A failed activation on either platform must not automatically delete its
evidence.

## Risk Register

| Risk | Mitigation | Gate |
| --- | --- | --- |
| Experimental development disrupts the production single rig | Protected artifact ledger, separate disabled-by-default namespace, read-only tests, explicit activation, and restore-before-cutover gate | Every milestone |
| Carla cannot change required objects quickly or transactionally | Prove the control boundary in Milestone 0; constrain early profiles to the current loaded graph | M0, M6 |
| Current 2048 quantum obscures latency claims | Measure commit and JACK adoption separately | Every benchmark |
| Instant switching conflicts with memory limits | Explicit readiness classes, pinning, memory ceiling, deterministic eviction | M6 |
| Shared M-VAVE USB IDs bind the wrong slot | Ordered semantic selectors and reconnect tests | M1, M4 |
| Physical faders cause sudden jumps | Required takeover policy and target-state tests | M1, M4 |
| Management pads become unreachable after switching | Persistent control layer and a reserved return action | M5 |
| Duplicate legacy and new routers emit twice | Shadow mode emits nothing; cut over one owner at a time | M3, M4 |
| Graph delta partially commits | Staging, verification, transaction log, and inverse rollback delta | M6 |
| Runtime callback performs unsafe work | Code review checklist, instrumentation, and callback isolation tests | M3 onward |
| Compiled cache does not match source | Fingerprints in compiled, active, status, and benchmark documents | M2 onward |
| Daemon crash loses state | Atomic persistence and safe fallback profile | M3 |
| Profile schema changes break stored state | Versioned schema and explicit migration before v2 activation | M1 onward |
| Linux-specific APIs leak into the portable core | Dual-platform builds, prohibited-header checks, and shared contract tests | M0-M3 |
| A required plugin or backend capability is absent on Windows | Semantic capability bindings, verified substitutions, and pre-activation rejection | M1, M7 |
| IPC, CLI, or state behavior drifts between platforms | Shared schemas, golden fixtures, bidirectional state tests, and one CLI contract | M3, M7 |

## Reviewable Change Sequence

The implementation should be delivered as small, independently reviewable
changes:

1. terminology decision and Milestone 0 benchmark harness;
2. schemas, fixtures, slots, and Hardware Presets;
3. current Device Profiles and `full-live-rack`;
4. deterministic compiler and parity report;
5. IPC protocol, CLI status/list, and daemon lifecycle;
6. JACK shadow adapter and current-service equivalence;
7. SMC-Mixer `eight-band-eq` cutover;
8. `multilevel-volume` and device/global CLI switching;
9. Arturia and SMK runtime cutovers;
10. MIDI management triggers;
11. prepared resource manager and native graph adapters;
12. first Arturia sound-design profile and prepared Rig Profile;
13. Windows adapters and complete-profile multiplatform certification; and
14. Linux and Windows installers, capture, validators, rollback, and legacy
    cleanup.

A change must not combine schema redesign, live routing cutover, and legacy
removal.

## Definition Of Done

The feature is complete when:

- the current setup is represented by versioned Rig and Device Profiles;
- deterministic compilation reproduces the verified current deployment;
- the portable core and CLI build from the same sources on Linux and Windows;
- `music-rig` exposes the same commands, protocol semantics, state format,
  error codes, and global/device switching behavior on both platforms;
- `music-rig` lists, validates, prepares, and switches global and device
  profiles;
- device overrides behave predictably and persist safely;
- Arturia management MIDI can invoke the same switch API;
- SMC-PAD and SMC-PAD Pocket can run different profiles concurrently;
- control-only and prepared switches meet measured targets;
- idle CPU and daemon RSS remain within their budgets;
- profile switching introduces no xruns or audio dropouts in the platform
  benchmarks;
- failure injection proves atomic rollback;
- one complete Rig Profile installs, validates, switches, restores state, and
  recovers on the Linux and Windows reference machines from the same authored
  definitions;
- platform-specific APIs, paths, backend identifiers, and service behavior stay
  inside adapters and bindings;
- capability reports expose substitutions and unsupported features, and
  activation rejects unmet requirements;
- the legacy or previous certified setup can be restored through the documented
  platform rollback command;
- the published support matrix records the certified backend and plugin
  capabilities on each platform; and
- English operational documentation describes cross-platform authoring,
  installation, switching, validation, performance measurement, and recovery.
