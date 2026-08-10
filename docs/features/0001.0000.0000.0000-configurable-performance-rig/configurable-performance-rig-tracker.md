# Configurable Performance Rig: Implementation Tracker

Feature: `0001.0000.0000.0000`

Updated: 2026-08-10

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
| GCC isolated suite | ✅ 17/17 passed |
| Clang isolated suite | ✅ 17/17 passed |
| GCC with Linux JSON probe | ✅ 18/18 passed |
| Clang with Linux JSON probe | ✅ 18/18 passed |
| Arturia offline parity | ✅ MIDI, state, replay, and stereo ramp passed |
| SMK-25 offline parity | ✅ Self-test, config, and JACK guard passed |
| Startup contract | ✅ Protected authority and read-only boundaries pass |
| Live startup transcript | 🟡 Planned; operator execution not performed |
| Switch benchmark contract | ✅ Valid fixture plus 11 negative semantic cases |
| Portable backend boundary | ✅ PipeWire and Carla names rejected from core |
| Linux JSON parse fixture | ✅ 10,000 iterations, zero failures |
| Linux JSON average parse | ✅ GCC 10,858 ns; Clang 15,268 ns |
| Linux JSON linkage | ✅ Default CLI has no `json-c` dependency |
| Protected baseline | ✅ 30/30 checks passed |
| Linux IPC round trips | ✅ 1,000 requests, zero failures |
| Linux IPC latency | ✅ p99 39,153 ns; maximum 395,453 ns |
| Live graph or service changes | ✅ None |

IPC and JSON timings are local Milestone 0 spikes, not production-daemon
benchmarks.

## Milestone Summary

| Milestone | State | Exit gate |
| --- | --- | --- |
| 0. Baseline and technical gates | 🟡 | Technical spikes and reference evidence incomplete |
| 1. Schemas and current profile extraction | ⬜ | Not started |
| 2. Deterministic compiler and parity | ⬜ | Not started |
| 3. Runtime, CLI, and shadow mode | ⬜ | Not started |
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
- 🟡 Capture the Airstar baseline. The current report matches every protected
  link but also observes one `smc-pad-gain-verify` diagnostic link.
- ✅ Exercise and record the Arturia and SMK helper
  [offline tests](../../tools/music-rig/HELPER-OFFLINE-TESTS.md).
- 🟡 Record the repeatable current-rack startup and validation transcript.
  The deterministic [planned transcript](../../tools/music-rig/CURRENT-RACK-STARTUP.md)
  passes drift checks; operator-controlled live evidence remains.

### Performance And Runtime

- ✅ Define benchmark JSON output and timing boundaries. The portable
  [contract](../../tools/music-rig/benchmarks/SWITCH-BENCHMARK-CONTRACT.md)
  enforces distinct commit/adoption measurements, three load scenarios,
  percentiles, resource separation, stability deltas, and raw-evidence rules.
- ✅ Build the minimal portable C17 core and CLI on Linux.
- ⬜ Build and test the same core and CLI on Windows.
- ✅ Prove portable versioned frames and Linux `SOCK_SEQPACKET` round trips.
- ⬜ Prove the same golden frames through the Windows candidate transport.
- 🟡 Prove `json-c` availability and footprint on Linux and Windows. Linux
  passes with 0.17; Windows remains pending.
- ✅ Reject platform headers and symbols from the portable core.
- ✅ Prove lock-free generation publication and synthetic callback adoption.
- ⬜ Prove generation adoption on the Windows reference build.

### Architecture Decisions

- ⬜ Select and record the Windows reference machine.
- 🟡 Evaluate local named pipes in message mode as the Windows IPC candidate;
  the round-trip proof and final decision remain pending.
- ✅ Decide whether native PipeWire graph control can wait until Milestone 6.
  [ADR 0001](architecture-decisions/0001-pipewire-graph-control-deferral.md)
  defers mutation; pre-Milestone-6 control-only switches require an empty graph
  delta.
- ✅ Record the supported Carla live-control and prepared-engine boundary.
  [ADR 0002](architecture-decisions/0002-carla-control-and-prepared-engine-boundary.md)
  keeps early switches on loaded engines and moves lifecycle, state, project,
  and patchbay work to preparation in Milestone 6.
- 🟡 Write the remaining Milestone 0 architecture decision records. IPC,
  provisional JSON parsing, and PipeWire deferral boundaries are recorded;
  the Windows backend remains.
- ⬜ Confirm or revise CPU, memory, latency, and xrun thresholds.

## Milestone 1: Schemas And Current Profile Extraction

- ⬜ Add versioned Rig, Rig Profile, Device Profile, Hardware Preset, trigger,
  common, and platform-binding schemas.
- ⬜ Define stable device slots and ordered physical selectors.
- ⬜ Extract the five current Device Profiles and their Hardware Presets.
- ⬜ Add the `full-live-rack` Rig Profile.
- ⬜ Model ownership, semantic capabilities, readiness, and takeover policies.
- ⬜ Add Linux bindings and Windows contract fixtures.
- ⬜ Add schema, reference, collision, ownership, and ambiguity tests.

## Milestone 2: Deterministic Compiler And Parity

- ⬜ Implement deterministic source compilation and fingerprints.
- ⬜ Compile semantic mappings, ownership, and graph deltas.
- ⬜ Materialize Carla and Patchbay output only in temporary test locations.
- ⬜ Prove 49-plugin, 111-project-connection, and 115-link semantic parity.
- ⬜ Add relocation, missing-capability, repeatability, and regression tests.

## Milestone 3: Runtime, CLI, And Shadow Mode

- ⬜ Implement the portable daemon control loop, state, metrics, and adapters.
- ⬜ Implement the complete versioned IPC and CLI read-only/dry-run commands.
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
