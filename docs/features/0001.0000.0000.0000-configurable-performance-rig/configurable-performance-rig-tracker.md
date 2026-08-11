# Configurable Performance Rig: Implementation Tracker

Feature: `0001.0000.0000.0000`

Updated: 2026-08-11

Documents:

- [Feature proposal](configurable-performance-rig.md)
- [Proposta de funcionalidade (pt-BR)](configurable-performance-rig.pt-BR.md)
- [Implementation plan](configurable-performance-rig-implementation-plan.md)
- [Architecture decisions](architecture-decisions/README.md)

## Status Key

| Mark | Meaning |
| --- | --- |
| ✅ | Complete and verified |
| 🟡 | In progress or partially verified |
| ⬜ | Not started |
| ⛔ | Blocked by a named dependency |

## Safety Lock

- ✅ Protected single-rig artifacts remain the production authority.
- ✅ Experimental code has no install or activation target.
- ✅ Automated tests do not mutate the live audio or MIDI graph.
- ✅ Restore remains preview-only without the explicit `--apply` option.
- ✅ Airstar observations create no remote files and control no services.
- ⬜ Production restore rehearsal is required before the first live cutover.

## Latest Verification

| Check | Result |
| --- | --- |
| GCC isolated suite | ✅ 35/35 passed |
| Clang isolated suite | ✅ 35/35 passed |
| GCC with Linux JSON adapter | ✅ 38/38 passed |
| Clang with Linux JSON adapter | ✅ 38/38 passed |
| Arturia offline parity | ✅ MIDI, state, replay, and stereo ramp passed |
| SMK-25 offline parity | ✅ Self-test, config, and JACK guard passed |
| Startup contract | ✅ Protected authority and read-only boundaries pass |
| Live startup transcript | ✅ [Passing operator run](../../tools/music-rig/benchmarks/current-rack-startup-2026-08-10.md); five controllers, zero validation failures |
| Switch benchmark contract | ✅ Valid fixture plus 13 negative semantic cases |
| Acceptance thresholds | ✅ Selected; platform measurements remain future gates |
| Windows hosted build | ✅ [Windows 2025 MSVC run](https://github.com/pedrofmj/music-studies/actions/runs/31414189191), 10/10 default and 11/11 JSON-enabled passed |
| Windows reference machine | ✅ `beanstar` selected and [inventoried](../../tools/music-rig/WINDOWS-REFERENCE.md) |
| Windows generation adoption | ✅ 9,999 publications; 200 ns maximum commit on `beanstar` |
| Windows named-pipe round trips | ✅ 1,000 requests; p99 17,400 ns; zero failures on `beanstar` |
| Portable backend boundary | ✅ PipeWire and Carla names rejected from core |
| Linux JSON parse fixture | ✅ 10,000 iterations, zero failures |
| Linux JSON average parse | ✅ GCC 10,858 ns; Clang 15,268 ns |
| Linux JSON linkage | ✅ Default CLI has no `json-c` dependency |
| Windows JSON parse fixture | ✅ 10,000 iterations; 16,051 ns average; zero failures on `beanstar` |
| Windows JSON footprint | ✅ 5,500,928-byte peak process working set; 26,112-byte static-link executable delta |
| Windows JSON packaging | ✅ Pinned vcpkg static archive, MIT license, and no `json-c` runtime DLL |
| Windows backend capability inventory | ✅ Read-only `beanstar` MIDI service, ASIO, Carla, and lifecycle snapshot recorded |
| Windows adapter baseline | ✅ WinMM, host-owned ASIO/WASAPI, Carla x64, Known Folders, QPC, and per-user Task Scheduler selected |
| Windows short-process resources | ✅ `music-rig --version`: 1,970,176-byte observed peak working set, 23 handles, 1 thread, clean exit |
| Windows 60-second idle resources | ✅ 60.259 s, 0.000000% measured child CPU, 3,670,016-byte observed peak working set, 65 handles, 4 threads, zero events |
| Windows resource cleanup | ✅ Zero remaining test processes; temporary directory removed and independently confirmed |
| Portable profile schemas | ✅ [Linux 23/23, Windows 17/17, and Windows JSON 18/18](https://github.com/pedrofmj/music-studies/actions/runs/31486826609) pass with five verified Hardware Presets, zero partial presets, five Device Profiles, one Rig Profile, one Linux Platform Binding, one inactive Switch Trigger document, six valid fixtures, fourteen invalid fixtures, and twenty-eight catalogue cases |
| Deterministic compiler runtime tables | ✅ [Linux 25/25, Windows 19/19, and Windows JSON 20/20](https://github.com/pedrofmj/music-studies/actions/runs/31494175730) pass; 5 inputs, 72 mappings, 71 selected targets, 57 ownership entries, empty current graph delta, and 13 compiler cases pass on both platforms |
| Milestone 2 regression gate | ✅ [Linux 28/28, Windows 22/22, and Windows JSON 23/23](https://github.com/pedrofmj/music-studies/actions/runs/31514575263) pass; relocation, repeatability, missing plugin/asset/endpoint diagnostics, 18 asset references, and all 25 protected checksums pass |
| Portable runtime control loop | ✅ [Linux 32/32, Windows 26/26, and Windows JSON 27/27](https://github.com/pedrofmj/music-studies/actions/runs/31521502726) pass; fixed-storage state, saturating metrics, generation conflicts, adapter failures, output suppression, and inert daemon startup are covered |
| Qualified definition and state loading | ✅ [Linux 34/34, Windows 28/28, and Windows JSON 30/30](https://github.com/pedrofmj/music-studies/actions/runs/31526121270) pass; bounded compiled metadata, 64-byte state integrity, restore/fallback, storage failures, and inert output behavior are covered |
| Native explicit-path storage | ✅ [Linux 35/35, Windows 29/29, and Windows JSON 32/32](https://github.com/pedrofmj/music-studies/actions/runs/31531477956) pass; UTF-8 definition reads, native atomic state replacement, cleanup, missing paths, fingerprint mismatch, and offline daemon loading are covered |
| Protected baseline | ✅ 30/30 checks passed |
| Linux IPC round trips | ✅ 1,000 requests, zero failures |
| Linux IPC latency | ✅ p99 39,153 ns; maximum 395,453 ns |
| Authorized live graph test | ✅ Diagnostic link temporarily disconnected, clean capture passed, exact link restored; no service changes |

IPC and JSON timings are local Milestone 0 spikes, not production-daemon
benchmarks.

## Milestone Summary

| Milestone | State | Exit gate |
| --- | --- | --- |
| 0. Baseline and technical gates | ✅ | Baseline reproducible and technical gates passed |
| 1. Schemas and current profile extraction | ✅ | `full-live-rack` describes every current role and dependency; all known conflict classes are rejected |
| 2. Deterministic compiler and parity | ✅ | Authored definitions deterministically materialize an equivalent temporary deployment on Linux and Windows; the protected live setup remains unchanged |
| 3. Runtime, CLI, and shadow mode | 🟡 | Portable output-suppressed control loop, qualified state/definition contracts, explicit-path native storage, and inert daemon target pass on Linux and Windows; production path selection, complete adapters, CLI/IPC, mapping tables, and shadow execution remain |
| 4. Control-only switching | ⬜ | Not started |
| 5. MIDI management triggers | ⬜ | Not started |
| 6. Prepared engines and graph deltas | ⬜ | Not started |
| 7. Windows certification | ⬜ | Not started |
| 8. Deployment cutover and cleanup | ⬜ | Not started |

## Milestone 0: Baseline And Technical Gates

### Stable Baseline

- ✅ Record the immutable protected-baseline manifest.
- ✅ Add the read-only protected-baseline verifier.
- ✅ Add the preview-first one-command restore path.
- ✅ Document the explicit opt-in boundary for live experiments.
- ✅ Capture the Airstar baseline. The recorded report matches every protected
  Carla, graph, audio, and service check with no missing or unexpected links.
- ✅ Capture a clean transient-free Airstar observation through the explicit
  live-change approval path. The one diagnostic link was temporarily
  disconnected, the capture passed, and the exact link was restored and
  verified afterward without changing a service or protected artifact.
- ✅ Exercise and record the Arturia and SMK helper
  [offline tests](../../tools/music-rig/HELPER-OFFLINE-TESTS.md).
- ✅ Record the repeatable current-rack startup and validation transcript. The
  deterministic [planned transcript](../../tools/music-rig/CURRENT-RACK-STARTUP.md)
  and [2026-08-10 operator run](../../tools/music-rig/benchmarks/current-rack-startup-2026-08-10.md)
  pass source, installed-rack, hardware, launch, graph, musical, and stability
  checks.

### Performance And Runtime

- ✅ Define benchmark JSON output and timing boundaries. The portable
  [contract](../../tools/music-rig/benchmarks/SWITCH-BENCHMARK-CONTRACT.md)
  enforces distinct commit/adoption measurements, three load scenarios,
  percentiles, resource separation, stability deltas, and raw-evidence rules.
- ✅ Build the minimal portable C17 core and CLI on Linux.
- ✅ Build and test the same core and CLI on Windows. The pinned Windows 2025
  [hosted workflow](../../../.github/workflows/music-rig-portable.yml) passed
  all ten configured tests in
  [run 31399835507](https://github.com/pedrofmj/music-studies/actions/runs/31399835507).
- ✅ Prove portable versioned frames and Linux `SOCK_SEQPACKET` round trips.
- ✅ Prove the same golden frames through the Windows candidate transport. The
  hosted test and hash-verified `beanstar` artifact pass 1,000 message-mode
  round trips with zero failures and a 17,400 ns p99.
- ✅ Prove `json-c` availability and footprint on Linux and Windows. Linux 0.17
  and Windows 0.18 pass the same fixture. The pinned static Windows bundle
  passed hosted MSVC tests and the hash-verified `beanstar` footprint probe;
  [ADR 0005](architecture-decisions/0005-json-c-control-plane-parsing.md)
  accepts the control-plane-only boundary.
- ✅ Reject platform headers and symbols from the portable core.
- ✅ Prove lock-free generation publication and synthetic callback adoption.
- ✅ Prove generation adoption on the Windows reference build. The
  hash-verified native artifact completed 9,999 publications on `beanstar`,
  remained lock-free, and recorded a 200 ns maximum control commit.
- ✅ Record reliable Windows short-process and 60-second zero-event synthetic
  daemon resource measurements on `beanstar`, including working set, idle CPU,
  threads, handles, shutdown, and cleanup. The hash-verified
  [physical result](../../tools/music-rig/benchmarks/windows-resource-beanstar.json)
  passed every gate and left no process or temporary directory behind.

### Architecture Decisions

- ✅ Select and record the Windows reference machine. `beanstar` and its
  initial read-only hardware, OS, and media-driver inventory are recorded in
  [WINDOWS-REFERENCE.md](../../tools/music-rig/WINDOWS-REFERENCE.md).
- ✅ Select local named pipes in message mode as the Windows IPC transport.
  Hosted and physical golden-frame round trips pass; explicit ACL, remote
  rejection, timeout, and production-rights requirements are recorded in
  [ADR 0004](architecture-decisions/0004-windows-local-ipc-named-pipes.md).
- ✅ Decide whether native PipeWire graph control can wait until Milestone 6.
  [ADR 0001](architecture-decisions/0001-pipewire-graph-control-deferral.md)
  defers mutation; pre-Milestone-6 control-only switches require an empty graph
  delta.
- ✅ Record the supported Carla live-control and prepared-engine boundary.
  [ADR 0002](architecture-decisions/0002-carla-control-and-prepared-engine-boundary.md)
  keeps early switches on loaded engines and moves lifecycle, state, project,
  and patchbay work to preparation in Milestone 6.
- ✅ Write the Milestone 0 architecture decision records. IPC, JSON parsing,
  PipeWire, Carla, performance thresholds, and the Windows adapter baseline are
  accepted. [ADR 0006](architecture-decisions/0006-windows-platform-adapter-baseline.md)
  keeps physical Windows certification in Milestone 7.
- ✅ Confirm CPU, memory, latency, and xrun thresholds.
  [ADR 0003](architecture-decisions/0003-performance-acceptance-thresholds.md)
  accepts the portable ceilings and separates selection from later platform
  certification. Idle CPU now requires a 60-second zero-event observation.

## Milestone 1: Schemas And Current Profile Extraction

- ✅ Add the seven versioned Rig, Rig Profile, Device Profile, Hardware Preset,
  Platform Binding, Switch Trigger, and common schemas. The offline
  [validator](../../tools/music-rig/validate-performance-rig.py) checks each
  Draft 2020-12 schema and twenty positive/negative fixtures on the default
  CTest path without activating any runtime component.
- ✅ Define stable device slots and ordered physical selectors. The authored
  [rig catalogue](../../../src/performance-rigs/pedro-performance-rig/rig.json)
  records all five current controllers, required endpoint purposes, unique
  semantic aliases and local discriminators, and optional USB evidence.
- ✅ Extract the current Hardware Presets. All five
  [preset documents](../../../src/performance-rigs/pedro-performance-rig/hardware-presets/)
  resolve from the Rig catalogue and are verified. The
  [Airstar capture](../../tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json)
  records every current SMC-PAD MIDI pad, the Pocket's eight MIDI pads and
  eight silent internal controls, exact cleanup, and normal post-capture audio.
- ✅ Extract the five current Device Profiles. The
  [portable definitions](../../../src/performance-rigs/pedro-performance-rig/device-profiles/)
  preserve the Arturia multi-instrument rack, SMK-25 ambient pad layers,
  SMC-Mixer eight-band equalizer, and both SMC-PAD drum roles without platform
  paths or backend-specific identifiers.
- ✅ Add the `full-live-rack` Rig Profile. The portable
  [composition](../../../src/performance-rigs/pedro-performance-rig/rig-profiles/full-live-rack.json)
  selects all five current Device Profiles, pins the eighteen current engines,
  and records the shared drum engine and effects without activating a runtime.
- ✅ Model ownership, semantic capabilities, readiness, and takeover policies.
  Explicit Rig Profile validation now covers mandatory slots, selected-profile
  references, aggregate capabilities and readiness, pinned and shared
  resources, initial-state ownership, and composed ownership conflicts. The
  pads' shared drum-note target remains valid while exclusive overlap fails.
- ✅ Add Linux bindings and Windows contract fixtures. The authoring-only
  [`airstar-current`](../../../src/performance-rigs/pedro-performance-rig/platform-bindings/linux/airstar-current.json)
  binding resolves all five slots and every `full-live-rack` capability and
  resource against protected Airstar aliases, targets, paths, checksums, and
  services. A Windows fixture proves WinMM, Known Folder, Carla, and Task
  Scheduler contract shapes without claiming physical Windows support.
- ✅ Add schema, reference, collision, ownership, and ambiguity tests. Schema
  structure, portability-negative coverage, selector order, required endpoint
  coverage, shared USB ambiguity, Hardware Preset and Device Profile
  references, source controls, per-preset MIDI collisions, mapping ownership,
  Rig Profile resolution and composition, ownership conflicts, binding
  coverage, missing-capability diagnostics, unknown targets, fixture isolation,
  cross-platform leakage, unique management events, trigger source and
  operation resolution, Hardware Preset assignment, readiness ceilings, and
  trigger-versus-musical-mapping conflicts pass. Consumed overlaps fail while
  explicit passthrough remains valid. The authored management catalogue is
  resolved but empty, so no MIDI trigger is active.

## Milestone 2: Deterministic Compiler And Parity

- ✅ Implement deterministic source compilation and fingerprints. The
  authoring-only compiler emits canonical UTF-8 JSON with independent schema,
  portable-source, Platform Binding, and generated-definition SHA-256
  fingerprints. Thirteen cases cover exact golden bytes, repeatability across
  working directories, fingerprint recomputation, source-overwrite refusal,
  no-write mode, deterministic diagnostics, binding isolation, runtime tables,
  ownership, graph classification, and relative encoding.
- ✅ Compile semantic mappings, ownership, and graph deltas. The compiler emits
  5 stable-slot input bindings, 72 mapping rows plus a direct dispatch index,
  71 selected semantic target bindings, and 57 consolidated ownership entries.
  The current graph delta has zero created/removed links, created/removed
  objects, and metadata changes; nonempty control-only deltas are rejected.
  Local GCC and Clang pass 25/25; hosted Linux passes 25/25, Windows passes
  19/19, and the Windows JSON-enabled regression passes 20/20.
- ✅ Materialize Carla and Patchbay output only in temporary test locations.
  The guarded authoring command produces a deterministic three-file bundle,
  refuses non-temporary and unsafe definitions before output creation, proves
  combined, home, SoundFont-only, and identity relocation, and leaves the
  protected baseline at 30/30. Its thirteen-case self-test passes in the full
  local GCC and Clang suites at 28/28. Hosted
  [run 31514575263](https://github.com/pedrofmj/music-studies/actions/runs/31514575263)
  passes Linux 28/28, Windows 22/22, and the Windows JSON-enabled suite 23/23.
- ✅ Prove 49-plugin, 111-project-connection, and 115-link semantic parity.
  The read-only gate matches full path-normalized plugin subtrees, eighteen
  plugin asset references, exact Carla connection pairs, and stable Patchbay
  endpoint selectors rather than counts alone. Its ten-case mutation suite
  passes in the full local GCC and Clang suites at 28/28, and the protected
  baseline remains 30/30. Hosted
  [run 31514575263](https://github.com/pedrofmj/music-studies/actions/runs/31514575263)
  passes Linux 28/28, Windows 22/22, and the Windows JSON-enabled suite 23/23.
- ✅ Add relocation, missing-capability, repeatability, and regression tests.
  Compiler diagnostics reject missing bindings; materializer and parity tests
  reject missing endpoints, plugins, and assets with deterministic messages.
  Exact relocation and repeatability pass across working directories and CRLF
  checkouts. The portable protected-single-rig regression verifies all 25
  protected checksums, legacy Python and shell validation entry points,
  controlled missing-asset diagnostics, read-only behavior, and unchanged
  sources. The compiler check and legacy regression run together in CTest
  without editing the production installer or entering its installation,
  service-lifecycle, or live-graph paths.

## Milestone 3: Runtime, CLI, And Shadow Mode

- 🟡 Implement the portable daemon control loop, state, metrics, and adapters.
  The fixed-storage loop provides versioned lifecycle state, saturating metrics,
  expected-generation publication, and ABI-versioned clock/control/storage
  boundaries. A bounded loader validates the current compiled envelope metadata
  and prepares caller-owned immutable tables for all 5 profile/input bindings,
  72 mappings, 71 targets, and 57 ownership entries. It cross-checks every
  compiler mapping-index entry and builds a fixed 256-entry numeric event
  dispatch index with no lookup allocation, lock, JSON traversal, or string
  comparison. The complete version 1 table image is 451,032 bytes on the local
  64-bit builds. The exact 64-byte persistent-state frame detects corruption and
  covers qualified restore, definition-fingerprint fallback, and atomic-replace
  failure through mock storage. Explicit-path host adapters read definitions and
  state on Linux and Windows and atomically replace only state through native
  same-directory temporary files. The opt-in daemon can load and report one
  trusted definition through an explicit offline command. Local GCC/Clang pass
  35/35, both optional JSON builds pass 38/38, seven ASan/UBSan boundary tests
  pass, and the protected baseline remains 30/30. Hosted
  [run 31536417870](https://github.com/pedrofmj/music-studies/actions/runs/31536417870)
  validates the immutable tables on Linux 35/35, Windows 29/29, and Windows JSON
  32/32. `music-rigd` remains output-suppressed and inert without an explicit
  command. Production XDG/Known Folder selection and the device/MIDI,
  audio/graph, plugin-host, service, and diagnostics adapters remain.
- 🟡 Implement the complete versioned IPC and CLI read-only/dry-run commands.
  Protocol v2 freezes all nine operation IDs in fixed 176-byte requests and
  2,592-byte bounded responses. The portable dispatcher and CLI cover status,
  filtered profile listing, validation, and global/device dry-runs while
  rejecting every commit request. Ephemeral Linux and Windows transports carry
  real table-backed requests; the executable has no configured endpoint. Local
  GCC/Clang pass 38/38, both optional JSON builds pass 41/41, eight ASan/UBSan
  boundary tests pass, and the protected baseline remains 30/30. Hosted Windows
  proof is pending.
- ⬜ Add immutable-generation reclamation and stable device-slot ports.
- ⬜ Extract reusable behavior without changing installed legacy services.
- ⬜ Run the new adapter in output-suppressed shadow mode.
- ⬜ Prove Linux and Windows protocol, state, parity, and resource behavior.

## Milestone 4: Control-Only Switching

- ⬜ Cut over SMC-Mixer only after independent parity validation.
- ⬜ Implement `eight-band-eq` and `multilevel-volume`.
- ⬜ Support independent SMC-PAD and SMC-PAD Pocket profiles.
- ⬜ Implement device and global CLI switching with overrides and reset.
- ⬜ Prove atomicity, takeover safety, reconnect behavior, and rollback.
- ⬜ Meet switch latency, resource, and xrun thresholds.

## Milestone 5: MIDI Management Triggers

- ⬜ Capture and reserve exact Arturia management controls.
- ⬜ Compile persistent CC, note, and program-change triggers.
- ⬜ Queue requests from the callback without executing switches there.
- ⬜ Reuse the same transaction API used by the CLI.
- ⬜ Prove debounce, consume/passthrough, recursion safety, and latency.

## Milestone 6: Prepared Engines And Graph Deltas

- ⬜ Implement the selected PipeWire and Carla control adapters.
- ⬜ Implement preparation, readiness, pinning, and bounded eviction.
- ⬜ Add staged graph deltas, verification, ramps, and full rollback.
- ⬜ Add one Arturia sound-design Device Profile.
- ⬜ Add a materially different prepared Rig Profile.
- ⬜ Prove switching stability within CPU and memory budgets.

## Milestone 7: Windows Certification

- ⬜ Implement Windows IPC, clock, path, service, MIDI, audio, and host adapters.
- ⬜ Bind one complete Rig Profile without changing portable authored profiles.
- ⬜ Add Windows installation, validation, rollback, and capability reporting.
- ⬜ Prove CLI, MIDI, state, reconnect, and cross-platform compatibility.
- ⬜ Record Windows-specific performance and resource thresholds.

## Milestone 8: Deployment Cutover And Cleanup

- ⬜ Rehearse restore and rollback before any ownership change.
- ⬜ Promote only independently validated device ownership.
- ⬜ Complete soak, performance, failure-injection, and recovery validation.
- ⬜ Preserve legacy deployment until explicit production promotion.
- ⬜ Remove legacy ownership only after the final acceptance gate.
