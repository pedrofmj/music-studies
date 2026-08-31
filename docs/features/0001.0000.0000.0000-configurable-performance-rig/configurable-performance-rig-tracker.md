# Configurable Performance Rig: Implementation Tracker

Feature: `0001.0000.0000.0000`

Updated: 2026-08-31

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
- ✅ Production restore rehearsal passed on 2026-08-14. The apply restored the
  protected project, Patchbay snapshot, and services from verified sources with
  Carla closed, kept the pre-restore deployment and service evidence under
  `~/.local/state/pedro-live-rack/protected-restore-backups/20260814T233101`,
  and left all four legacy services enabled and active with zero validation
  failures.

## Latest Verification

| Check | Result |
| --- | --- |
| GCC isolated suite | ✅ 60/60 passed |
| Complete local schema-enabled suite | ✅ 63/63 passed on 2026-08-29 |
| Complete local JSON/JACK suite | ✅ 73/73 passed on 2026-08-29 |
| Output adoption acknowledgement | ✅ Atomic generation/timestamp and IPC rendering pass |
| Output-enabled switch benchmark | ✅ Three 1,000-sample scenarios pass commit and adoption gates |
| Offline SMC-MIX CC coalescing benchmark | ✅ 27,387 mapped inputs across 13,695 cycles; 13,695 emitted, 13,692 coalesced, 0 adapter failures |
| Airstar output-runtime rehearsal | ✅ Output-enabled IPC switches, adoption timestamps, zero graph mutation, and 0 live validation failures; [evidence](../../tools/music-rig/benchmarks/output-runtime-airstar-2026-08-29.json) |
| Airstar coalesced SMC-MIX no-link rehearsal | ✅ Protected verifier and legacy route passed before/during/after; relay registered 1 input/1 output, reported coalesced metrics, created 0 links, and cleaned up; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-no-link-rehearsal-airstar-2026-08-30.json) |
| Airstar coalesced SMC-MIX cutover/rollback no-input gate | 🟡 Corrected link transaction cut over to `0/8` legacy and `8/8` relay output links, then restored legacy `8/8`; exercise captured 0 MIDI events, so full live acceptance remains pending; [evidence](../../tools/music-rig/benchmarks/smc-mixer-cutover-rollback-no-input-airstar-2026-08-30.json) |
| Airstar coalesced SMC-MIX cutover/rollback repeat | 🟡 Cutover and rollback both passed with legacy `8/8` restored, but exercise captured 0 MIDI events and post-validation found 3/4 M-VAVE devices with 2 unresolved endpoints; [evidence](../../tools/music-rig/benchmarks/smc-mixer-cutover-rollback-repeat-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX cutover/rollback full-baseline repeat | 🟡 Four-device validation passed before/after and cutover/rollback restored legacy `8/8`, but the relay exercise captured 0 MIDI events; [evidence](../../tools/music-rig/benchmarks/smc-mixer-cutover-rollback-full-baseline-no-input-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay fader-1 partial exercise | 🟡 Relay received 186 CC40 events, emitted 35 latest updates, coalesced 151 intermediates, and rolled back cleanly; faders 2–8 were not evidenced; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-fader1-partial-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay fader-1 synchronized retest | 🟡 External capture recorded 2,844 probe events and 10,786 full-window events with zero PipeWire error delta, but the relay exited result 4 before metrics; rollback and final validation passed; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-fader1-high-rate-failure-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay fader-1 diagnostic retest | 🟡 Synchronized probe saw 2,734 CCs with zero PipeWire error delta; relay received 5,967 mapped events, emitted 1,217, coalesced 4,750, and had 0 adapter failures; faders 2–8 remain pending; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-fader1-diagnostic-retest-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay faders 1–8 fixed confirmation | ✅ Frame-ordered output passed synchronized physical movement on all eight faders: 4,225 mapped events, 833 emitted, 3,392 coalesced, 0 adapter failures, zero PipeWire error delta, clean rollback, and 0 final validation failures; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-faders1-8-fixed-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay faders 1–8 sustained soak | ✅ Five-minute synchronized movement passed all eight controls: 41,820 mapped events, 7,272 emitted, 34,548 coalesced, 0 adapter failures, zero PipeWire error delta, clean rollback, and 0 final validation failures; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-faders1-8-soak-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay audible-stability probe | 🟡 Correct physical endpoints captured 1,664 Arturia/pad events and the relay remained healthy with 0 adapter failures, but the 120-second window added 22 PipeWire `pw-top` ERR counts, including speaker sink +8; rollback and final validation passed; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-audible-stability-failure-airstar-2026-08-31.json) |
| Airstar relay-inactive audible-stability legacy baseline | ✅ Read-only 120-second comparison captured 2,756 events across SMC-MIX, SMC-PAD, and Arturia with zero PipeWire error delta and unchanged graph; the same workload is clean with the relay inactive; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-audible-stability-legacy-baseline-airstar-2026-08-31.json) |
| Airstar coalesced SMC-MIX relay audio-only isolation | ✅ Relay-active 120-second Arturia-only comparison captured 1,354 note events with zero relay input and zero PipeWire error delta; graph and rollback remained clean, showing relay presence alone did not reproduce the failure; [evidence](../../tools/music-rig/benchmarks/smc-mixer-relay-audio-only-airstar-2026-08-31.json) |
| Airstar output-runtime idle resources | ✅ 60 s, 0.4% max CPU, 12,616 KB peak RSS, 5 threads, 0 xrun/dropout/deadline events; [evidence](../../tools/music-rig/benchmarks/output-runtime-idle-airstar-2026-08-29.json) |
| Airstar protected-rack musical-load observation | 🟡 65 s, 1,314 live MIDI events, graph unchanged, and 0 validator failures; speaker-sink `pw-top` ERR increased by 4, so the zero-xrun gate did not pass; [evidence](../../tools/music-rig/benchmarks/protected-rack-musical-load-airstar-2026-08-29.json) |
| Airstar synthetic note-only isolation | 🟡 65 s, 753 generated note messages, no SMC-MIX EQ counter increase, and graph restored; speaker-sink `pw-top` ERR increased by 1, so the zero-xrun gate remains open; [evidence](../../tools/music-rig/benchmarks/synthetic-notes-airstar-2026-08-30.json) |
| Airstar settled synthetic note-only observation | ✅ 65 s steady-state note stream, 780 generated messages, no sink/Carla/EQ error-counter delta, zero journal errors, and graph restored; [evidence](../../tools/music-rig/benchmarks/synthetic-notes-settled-airstar-2026-08-30.json) |
| Airstar Flatpak Carla isolation attempt | 🟡 Carla copy blocked by Flatpak/JACK incompatibility; fallback SMC-MIX/JACK harness passed 5/5 without touching the live graph; [evidence](../../tools/music-rig/benchmarks/smc-mixer-offline-reproduction-airstar-2026-08-30.json) |
| Airstar native offline Carla CC reproduction | ✅ 65 s, 2,816 mapped CC events through all eight SMC-EQ stages, 0 offline JACK xruns/errors, and no live graph change; [evidence](../../tools/music-rig/benchmarks/smc-mixer-offline-carla-cc-airstar-2026-08-30.json) |
| Airstar live PipeWire duplicate-Carla CC isolation | 🟡 2,816 CC events reached 8/8 duplicate stages, but 49 temporary Carla nodes perturbed cumulative live error counters and invalidated xrun attribution; graph restored and validator passed; [evidence](../../tools/music-rig/benchmarks/live-pipewire-duplicate-carla-cc-isolation-airstar-2026-08-30.json) |
| Airstar SMC-MIX control/xrun correlation | ✅ 60 s, 2,643 control changes on `20:1`, zero changed `pw-top` nodes, zero journal matches, and unchanged graph hash; [evidence](../../tools/music-rig/benchmarks/smc-mixer-control-correlation-airstar-2026-08-30.json) |
| Airstar SMC-MIX no-touch correlation repeat | ✅ 60 s with no device interaction, 0 MIDI events, zero changed `pw-top` nodes, zero journal matches, and unchanged graph hash; [evidence](../../tools/music-rig/benchmarks/smc-mixer-control-correlation-no-touch-airstar-2026-08-30.json) |
| Airstar SMC-MIX intentional fader sweep | ✅ 60 s, 7,132 mirrored control arrivals (`3,566` per port), approximately 93.5 events/s per port, zero changed `pw-top` nodes, zero journal matches, and unchanged graph hash; [evidence](../../tools/music-rig/benchmarks/smc-mixer-fader-sweep-correlation-airstar-2026-08-30.json) |
| Airstar mixed musical/control soak | 🟡 300 s observation, 2,084 arrivals (`2,030` mirrored mixer CCs and `54` Arturia notes), zero changed `pw-top` nodes, zero journal matches, and unchanged graph hash; activity occupied only 7.755242 s; [evidence](../../tools/music-rig/benchmarks/mixed-musical-control-soak-airstar-2026-08-30.json) |
| Airstar sustained mixed musical/control soak | 🟡 300 s, 6,592 arrivals across 229.669036 active seconds, but sink `ERR +5` and SMC-MIX EQ `ERR +3`; graph hash unchanged and validator passed; [evidence](../../tools/music-rig/benchmarks/sustained-mixed-control-soak-airstar-2026-08-30.json) |
| Airstar sustained Arturia-only note load | ✅ 300 s, 4,167 Arturia-port events across 299.144290 active seconds, zero changed `pw-top` nodes, zero journal matches, and unchanged graph hash; [evidence](../../tools/music-rig/benchmarks/arturia-only-sustained-note-load-airstar-2026-08-30.json) |
| Airstar sustained SMC-MIX-only control load | 🟡 300 s, 54,774 mirrored CC arrivals across 287.881699 active seconds, but sink `ERR +1` and SMC-MIX EQ `ERR +2`; graph hash unchanged and validator passed; [evidence](../../tools/music-rig/benchmarks/smc-mixer-only-sustained-control-airstar-2026-08-30.json) |
| Clang isolated suite | ✅ 60/60 passed |
| GCC with Linux JSON/JACK adapters | ✅ 67/67 passed |
| Clang with Linux JSON/JACK adapters | ✅ 67/67 passed |
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
| Protocol v2 read-only/dry-run control | ✅ [Linux 38/38, Windows 32/32, and Windows JSON 35/35](https://github.com/pedrofmj/music-studies/actions/runs/31539454955) pass; fixed frames, nine operation IDs, fail-closed CLI, table-backed dispatch, and Linux/Windows mock transports are covered |
| Bounded generations and stable ports | ✅ [Linux 39/39, Windows 33/33, and Windows JSON 36/36](https://github.com/pedrofmj/music-studies/actions/runs/31546338374) pass; four slots sustain 9,999 publications with 9,999 reclamations and at most three retired; ten current-Rig port identities are stable |
| Device/MIDI output-suppressed shadow | ✅ [Linux 51/51, Windows 38/38, and Windows JSON 41/41](https://github.com/pedrofmj/music-studies/actions/runs/31596967924) pass; five-slot numeric dispatch, input-only JACK lifecycle, and suppressed Arturia/SMK-25 decisions are covered |
| Milestone 3 cross-platform closure | ✅ [Run 31637953686](https://github.com/pedrofmj/music-studies/actions/runs/31637953686) passes Linux 53/53, Windows 40/40, and Windows JSON 45/45; the checksummed evidence and seven-mutation validator pass on both platforms |
| Shadow idle resources | ✅ Linux: 0.000% CPU, 3,162,112 B peak observed RSS, 1 thread, 3 descriptors; Windows `beanstar`: 0.000% CPU, 6,381,568 B peak observed RSS, 4 threads, 94 handles; both complete the external 60-second zero-event window with one wait completion and clean shutdown |
| Airstar five-slot live shadow | ✅ 413-second normal-sound practice plus independent same-source Pocket comparison; every slot has input and mapping evidence, zero shadow output, zero attributable errors, and exact cleanup |
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
| 3. Runtime, CLI, and shadow mode | ✅ | Portable control/CLI/IPC, immutable mappings and behaviors, native storage, Linux host lifecycle, five-slot input-only live shadow, and Linux/Windows parity/resource proof pass; the protected rig is restored |
| 4. Control-only switching | 🟡 | First live cutover/rollback exercised 2026-08-15; relay route activation and rollback verified; eight-fader exercise completed; rollback verified; live validation passes |
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

- ✅ Implement the portable daemon control loop, state, metrics, adapter
  contracts, and explicit-path storage adapters.
  The fixed-storage loop provides versioned lifecycle state, saturating metrics,
  expected-generation publication, and ABI-versioned clock/control/storage
  boundaries. A bounded loader validates the current compiled envelope metadata
  and prepares caller-owned immutable tables for all 5 profile/input bindings,
  72 mappings, 71 targets, and 57 ownership entries. It cross-checks every
  compiler mapping-index entry and builds a fixed 256-entry numeric event
  dispatch index with no lookup allocation, lock, JSON traversal, or string
  comparison. The complete version 1 table image is 451,032 bytes on the local
  64-bit builds. The original v1 64-byte persistent-state frame detects
  corruption and covers qualified restore, definition-fingerprint fallback, and
  atomic-replace failure through mock storage; Milestone 4 retains v1 decoding
  while writing the active-profile-aware 128-byte v2 frame. Explicit-path host
  adapters read definitions and state on Linux and Windows and atomically replace
  only state through native same-directory temporary files. The opt-in daemon
  can load and report one trusted definition through an explicit offline command. Local GCC/Clang pass
  35/35, both optional JSON builds pass 38/38, seven ASan/UBSan boundary tests
  pass, and the protected baseline remains 30/30. Hosted
  [run 31536417870](https://github.com/pedrofmj/music-studies/actions/runs/31536417870)
  validates the immutable tables on Linux 35/35, Windows 29/29, and Windows JSON
  32/32. `music-rigd` remains output-suppressed and inert without an explicit
  command. Production host activation is intentionally tracked separately
  below; audio/graph and plugin-host work remains in Milestone 6, and complete
  Windows adapter certification remains in Milestone 7.
- ✅ Implement the complete versioned IPC and CLI read-only/dry-run commands.
  Protocol v2 freezes all nine operation IDs in fixed 176-byte requests and
  2,592-byte bounded responses. The portable dispatcher and CLI cover status,
  filtered profile listing, validation, and global/device dry-runs while
  rejecting every commit request. Ephemeral Linux and Windows transports carry
  real table-backed requests; the executable has no configured endpoint. Local
  GCC/Clang pass 38/38, both optional JSON builds pass 41/41, eight ASan/UBSan
  boundary tests pass, and the protected baseline remains 30/30. Hosted
  [run 31539454955](https://github.com/pedrofmj/music-studies/actions/runs/31539454955)
  passes Linux 38/38, Windows 32/32, and Windows JSON 35/35.
- ✅ Add immutable-generation reclamation and stable device-slot ports. A fixed
  eight-entry retirement ring applies backpressure without publishing when
  full, and control-thread reclamation makes caller storage reusable only after
  real-time adoption advances. Linux recycles four entries across 9,999
  publications, reclaims all 9,999 retired generations, and observes at most
  three retired entries. Validated tables derive two stable semantic port IDs
  per device slot; the current Rig has ten, profile-only changes preserve them,
  and a changed slot set fails before publication. No backend port is
  registered and output remains suppressed. Local GCC/Clang pass 39/39, both
  JSON builds pass 42/42, eight ASan/UBSan boundary tests pass, and the
  protected baseline remains 30/30. Hosted
  [run 31546338374](https://github.com/pedrofmj/music-studies/actions/runs/31546338374)
  passes Linux 39/39, Windows 33/33, and Windows JSON 36/36, including the
  bounded Windows reclamation and stable-port tests under `/W4 /WX`.
- ✅ Add production per-user path selection plus Linux lifecycle and
  rate-limited diagnostics adapters. The allocation-free resolver selects
  bounded XDG configuration, cache, state, and runtime paths without filesystem
  I/O. The fixed-storage portable limiter feeds an ABI-versioned structured
  stderr sink for journald. The explicit output-suppressed lifecycle blocks
  without polling, handles clean `SIGINT`/`SIGTERM` shutdown, and opens no
  definition, state, transport, MIDI, audio, graph, or plugin-host resource.
  The checked-in user unit is uninstalled and guarded by a separate opt-in
  marker. Local GCC/Clang pass 44/44, both optional JSON builds pass 47/47,
  18 ASan/UBSan runtime boundaries pass, and the protected baseline remains
  30/30. Hosted
  [run 31549413339](https://github.com/pedrofmj/music-studies/actions/runs/31549413339)
  passes Linux 44/44, Windows 34/34, and Windows JSON 37/37. Windows Known Folder
  and lifecycle completion remains part of Milestone 7.
- ✅ Extract reusable behavior without changing installed legacy services.
  Caller-owned Arturia and SMK-25 engines reproduce current event, connection,
  audio-gate, latch, transport, passthrough, and legacy-state transitions with
  fixed storage and no backend or host dependency. Local GCC/Clang pass 47/47,
  both optional JSON builds pass 50/50, 12 ASan/UBSan behavior/runtime
  boundaries pass, and the protected baseline remains 30/30. Hosted
  [run 31551820903](https://github.com/pedrofmj/music-studies/actions/runs/31551820903)
  passes Linux 47/47, Windows 36/36, and Windows JSON 39/39 under strict
  `/W4 /WX`. Linux offline differential tests compare the complete relevant
  state and emitted decisions directly with the protected sources, which remain
  unchanged.
- ✅ Implement the device/MIDI adapter and run it in output-suppressed shadow
  mode. The fixed 40,456-byte portable engine adopts immutable generations,
  dispatches numeric MIDI for all five current slots, and terminates Arturia and
  SMK-25 calculated messages at a suppression observer. Its event path has no
  allocation, lock, filesystem, JSON, or string lookup. The Linux JACK host
  opens with no-server-start, registers input ports only, and has no graph or
  MIDI-output API. Fake-JACK callback and end-to-end daemon processes cover
  activation, input, signal shutdown, failure cleanup, and zero output; the
  complete compiled envelope maps one event per current device. Local GCC passes
  51/51 and JSON/JACK 55/55, focused Clang and ASan/UBSan pass 8/8, and the
  protected baseline remains 30/30. The guarded local host attempt failed
  closed because no JACK server was available and left no port or link. Hosted
  [run 31596967924](https://github.com/pedrofmj/music-studies/actions/runs/31596967924)
  passes Linux 51/51, Windows 38/38, and Windows JSON 41/41 under strict
  `/W4 /WX`, including portable shadow compilation and five-slot JSON dispatch.
- ✅ Prove Linux and Windows protocol, state, parity, and resource behavior.
  The checksummed
  [Milestone 3 evidence](../../tools/music-rig/benchmarks/M3-SHADOW-EVIDENCE.md)
  binds the protocol v2 and 64-byte state contracts, the complete 5-input,
  72-mapping, 71-target, and 57-ownership compiled inventory, physical Linux
  and `beanstar` 60-second idle results, and the approved Airstar shadow session
  to one source commit and definition fingerprint. Both idle runs measured
  0.000% CPU and remained below 6.4 MB observed peak RSS, with zero input,
  mappings, or control work and one native wait completion. The 413-second live
  interval proved Arturia, SMK-25, SMC-Mixer, and SMC-PAD; its zero Pocket
  observation was rejected, then a same-source comparison independently proved
  Pocket input and mapping with every other shadow slot at zero. Shadow output
  remained unavailable, production sound remained normal, and zero PipeWire
  errors or dropouts were observed. All temporary links, ports, processes, and
  directories were removed; the Carla checksum, 117 graph links, 67 MIDI links,
  services, and restart counts remained unchanged; the protected post-check
  passed 30/30. Local GCC and Clang pass 53/53 and both JSON/JACK builds pass
  59/59. Hosted
  [run 31637953686](https://github.com/pedrofmj/music-studies/actions/runs/31637953686)
  passes Linux 53/53, Windows 40/40, and Windows JSON 45/45, including the
  cross-platform evidence validator and all seven negative mutations.

## Milestone 4: Control-Only Switching

- 🟡 Cut over SMC-Mixer only after independent parity validation. The portable
  relay exhaustively preserves all 1,024 mapped CC/value combinations from the
  exact compiled `eight-band-eq` contract. Its Linux JACK host exposes one
  fixed input/output pair without graph APIs, while a separate failure-safe
  transaction proves normal rollback plus injected failures in all three
  cutover phases. The ordinary daemon build has no output-capable command; only
  the explicit JSON/JACK build accepts the named definition, independently
  supplied fingerprint, output acknowledgement, and cutover acknowledgement.
  GCC passes 59/59, JSON/JACK passes 66/66, Clang passes 59/59, and the focused
  ASan/UBSan boundary passes 5/5. The protected baseline remains 30/30, and the
  Airstar preflight remains at eight legacy links and zero relay links. Hosted
  [run 31698846886](https://github.com/pedrofmj/music-studies/actions/runs/31698846886)
  passes Linux 59/59, Windows 43/43, and Windows JSON 48/48 under strict
  `/W4 /WX`, including the portable relay, all eight per-fader evidence
  counters, and absence of an output-capable command on Windows. The
  production restore rehearsal passed on 2026-08-14, the `smc-mixer-links`
  physical-output discovery was corrected so the Airstar legacy inventory
  verifies 8/8 with zero relay links, and live setup validation now passes 0
  failures (117/117 links) after restoring the missing SMK-25 AUX Stop/Play
  link and recapturing the protected baseline on 2026-08-14. The first live
  eight-fader cutover/rollback was exercised on 2026-08-15: the relay daemon
  started and registered its two-port inventory; `--cutover` activated the
  relay route (JACK topology verified: physical→relay input, relay output→8
  targets); operator exercised all eight faders; `--rollback` restored the
  legacy route (8/8 legacy links, 0 relay links); live validation passes 0
  failures (117/117). The explicitly approved live eight-fader cutover/rollback
  evidence is recorded in this item. A 2026-08-17 regression audit corrected
  the authored SMK-25 `midi.transport-input`, which had incorrectly reused the
  primary `SMK25-Master` alias instead of the distinct AUX `capture_2` alias.
  Current-catalogue validation now derives that endpoint from the protected
  Patchbay evidence, and compiler coverage locks the two endpoint locators.
  The live repair replaced the stale `SINCO 3 capture_2` route with the current
  SMK-25 `SINCO 4 capture_2` route and recaptured the deployed snapshot under a
  rollback guard; the graph backup is
  `patchbay-before-smk-transport-20260817T072442.json`. The Patchbay name
  resolver now correlates ambiguous AUX aliases with a uniquely named ALSA
  sequencer sibling on the same client, with a regression that reproduces the
  exact re-enumeration. Resolver deployment kept backup
  `pipewire-patchbay-json-before-identity-20260817T104035Z`; the old 117-link
  snapshot resolves 117/117 against the new graph, proving the repair survives
  re-enumeration. GCC and Clang pass 60/60, both JSON/JACK configurations pass
  67/67, the protected baseline passes 30/30, and full live Airstar validation
  passes 0 failures with all four services active and 117/117 links present. A
  2026-08-18 pad-audio regression was traced to retained channel-10 CC7=0 in the
  shared drum SoundFont. The protected Arturia helper now exposes a stateless
  `drum-volume-init` output that emits CC7=127 once on drum-target connection or
  reconnection. Offline coverage proves the exact `B9 07 7F` bytes, suppression
  during a steady connection, and replay after disconnect/reconnect. Guarded live
  deployment observed channel 10 CC7 value 127, removed the observer, and left the
  normalized performance snapshot at 116/116 with all four services active. The
  protected inventories are now 118 raw, 68 MIDI, and 116 deployment links.
- 🟡 Implement `eight-band-eq` and `multilevel-volume`. Both authored
  profiles compile deterministically. The active EQ relay has live parity
  evidence, and a bounded portable catalogue now validates prepared candidate
  definitions, rejects duplicate Rig Profile IDs and stable-port drift,
  lists its mixer profile as available, and accepts output-suppressed global
  and device dry-runs without changing the active generation. Output-enabled
  commit and rollback are covered with the mock adoption adapter; live profile
  adoption remains incomplete.
- 🟡 Support independent SMC-PAD and SMC-PAD Pocket profiles. The authored
  `pad-layer-controller` plus Pocket `drum-set` composition compiles in
  `multilevel-volume-mixed-pads`; runtime pad routing and activation remain.
- 🟡 Implement device and global CLI switching with overrides and reset. The
  parser and renderer cover global/device prepare and switch dry-runs plus
  device reset, including expected-generation guards and inactive prepared
  profile rendering. A validated device-table composition helper preserves
  stable input bindings and rebuilds profile-indexed mappings and dispatch data.
  The portable output-suppressed runtime now commits global and per-device
  prepared profiles with monotonic generation publication, v3 device-override
  persistence, v1/v2 state compatibility, base-profile switch-back,
  generation guards, reset commit, explicit persistence rollback, and
  rollback-failure reporting. A
  authenticated Linux production IPC endpoint is now implemented; output-enabled
  device-switch adoption and rollback are covered offline; live device adoption
  remains incomplete. The endpoint has offline
 filesystem-socket status, reconnect, serialized client takeover, and
  permission coverage. Offline GCC and Clang suites pass 61/61; JSON/JACK
  passes 69/69.
- ✅ Define the output-enabled transaction contract. The contract fixes phase
  order, monotonic rollback generations, backend rollback ownership, durable
  state ordering, response status, and fail-closed behavior in
  [OUTPUT-TRANSACTION.md](../../tools/music-rig/OUTPUT-TRANSACTION.md).
- 🟡 Implement the explicit Linux Device/MIDI output host. The fake-JACK gate
  proves paired stable-port registration, generated MIDI emission, adoption
  callbacks, shutdown, and registration-failure cleanup; default daemon
  activation remains opt-in. The guarded 2026-08-29 Airstar rehearsal passed
  output-enabled global/device/reset IPC transactions and real-time adoption;
  live backend promotion remains pending. The
  fake-JACK daemon process gate also proves explicit acknowledgement, startup,
  SIGTERM shutdown, paired-port registration, zero unsolicited output, and
  cleanup. The daemon lifecycle poll gate also proves automatic reconnect after
  backend shutdown and cleanup after replacement activation failure. The
  output-runtime process gate proves runtime enablement plus authenticated
  status, global switch, switch-back, persistence, and clean shutdown through
  the control socket. Its control poll also proves automatic backend reconnect
  before the next request. The output adapter reports the adopted generation
  and non-blocking `adopted_at_ns` timestamp over the same IPC status path.
- 🟡 Prove atomicity, takeover safety, reconnect behavior, and rollback.
  Global generation/persistence atomicity has injected rollback-success and
  rollback-failure coverage. The Linux daemon now has end-to-end commit,
  restart restoration, reset, malformed-peer recovery, serialized takeover,
  and active-socket refusal coverage. Live adoption, reconnect under an active
  device backend remain incomplete. Offline reconnect under an active device
  backend and the guarded Airstar output-runtime rehearsal are covered; live
  fault/reconnect evidence remains incomplete. Offline output-enabled rollback evidence
  covers confirmation failure, previous-generation restoration, monotonic
  rollback publication, and failed-runtime refusal.
- 🟡 Meet switch latency, resource, and xrun thresholds. The offline runtime
  benchmark now exercises 1,000 alternating device switch/reset transactions in
  both suppressed and output-enabled modes under idle, normal-performance, and
  synthetic high-load labels. It enforces the 20 ms control-commit p95 gate,
  reports non-blocking adoption timestamps, and enforces the one-processing-
  period-plus-margin atomic adoption bound. Daemon resource, xrun, dropout,
  and live-audio measurements are partly covered by the Airstar 60-second idle
  result. A 65-second operator-input observation captured 1,314 live MIDI events
  without graph mutation, but the speaker sink accumulated four additional
  `pw-top` ERR counts, so the zero-xrun musical-load gate remains unmet. Audible
  output and long-soak measurements remain pending. A bounded synthetic
  note-only isolation generated 753 messages without changing the EQ counter,
  but the speaker sink accumulated one further ERR count; the platform xrun
  count occurred during the unsettled link-activation phase. A second run that
  settled the temporary link before measurement generated 780 note messages
  with no sink, Carla, or EQ counter delta, proving steady-state note
  processing is stable. The operator/control-change xrun result, audible
  output, and long-soak measurements remain pending. The initial Flatpak
  Carla-copy attempt was blocked by its JACK boundary, but a native Ubuntu
  Carla 2.5.8 package extracted user-locally loaded the copied project and
  processed 2,816 mapped CC events through all eight SMC-EQ stages with zero
  offline JACK xruns/errors. The operator/control-change xrun, live audio,
  and long-soak gates remain unverified. A read-only SMC-MIX correlation window
  then captured 2,643 control changes with no changed `pw-top` node counters,
  no journal matches, and an unchanged graph hash; a later retry had zero input
  and is recorded as a no-touch baseline. A live-PipeWire duplicate-Carla
  attempt was rejected because its 49 temporary nodes perturbed cumulative
  live error counters and `pw-top` reported zeroed scheduler fields; it did
  not provide valid xrun attribution. A deliberate no-notes fader sweep then
  produced 7,132 mirrored control arrivals at roughly 93.5 events/s per port,
  including sub-millisecond bursts, with no PipeWire counter or journal delta;
  the earlier mixed-controller xrun remains a separate unresolved condition. A
  300-second mixed observation captured 2,030 mirrored mixer CCs and 54
  Arturia note events with no error-counter or journal delta, but all activity
  occupied only 7.755242 seconds, so sustained long-soak evidence remains
  pending. A subsequent 300-second sustained mixed run covered 229.669036
  active seconds and reproduced the live failure: the speaker sink gained five
  `pw-top` ERR counts and SMC-MIX EQ gained three, despite zero matching journal
  lines and exact graph restoration. The zero-xrun and long-soak gates remain
  open. A five-minute Arturia-only run then covered 299.144290 active seconds
  with 1,225 note-ons, 1,224 note-offs, and 1,718 Arturia-port CCs without any
  PipeWire counter or journal delta, narrowing the failure to SMC-MIX activity
  or its interaction with sustained musical input. A five-minute SMC-MIX-only
  run then produced 54,774 mirrored CC arrivals across 287.881699 active
  seconds and reproduced the fault without notes: SMC-MIX EQ gained two
  `pw-top` ERR counts and the speaker sink gained one. The live failure is now
  isolated to sustained mixer-control activity or its event-rate handling.
  The frame-ordered coalesced relay then passed a five-minute synchronized
  all-eight-fader soak with 79,834 mirrored CC arrivals, zero PipeWire error
  delta, 41,820 mapped relay inputs, 34,548 coalesced intermediates, zero
  adapter failures, and exact rollback. This closes the coalesced relay
  long-soak gate; audible output remains independently unverified, and the
  older direct-route failure remains recorded as a separate legacy-path
  condition. A subsequent two-minute operator probe with corrected Arturia and
  pad endpoints kept the relay at zero adapter failures but reproduced 22 live
  PipeWire error-counter increases, including speaker sink +8; that observer
  window contained no SMC-MIX events, so it does not attribute the failure to
  the relay. The audible stability gate therefore remains open pending a
  synchronized SMC-MIX-plus-audio probe.

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
