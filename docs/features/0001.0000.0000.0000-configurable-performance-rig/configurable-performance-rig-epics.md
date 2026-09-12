# Configurable Performance Rig: Epic Map

Feature: `0001.0000.0000.0000`

This document groups the implementation milestones into epics for sprint
planning. The implementation tracker remains the source of truth for detailed
evidence and acceptance status.

## Status Key

| Mark | Meaning |
| --- | --- |
| ✅ | Epic outcome complete and verified |
| 🟡 | Active or partially accepted |
| ⬜ | Planned, not started |
| ⛔ | Blocked by a named dependency or gate |

## Epic Summary

| Epic | Outcome | Status | Source milestones |
| --- | --- | --- | --- |
| E1. Protected Baseline And Observability | The production rig can be verified, observed, restored, and investigated without untracked mutation. | ✅ | 0 |
| E2. Portable Rig Definition | The complete rig, profiles, hardware presets, bindings, ownership, and readiness are authored and validated portably. | ✅ | 1 |
| E3. Deterministic Materialization | Authored definitions compile into reproducible runtime tables and temporary parity materializations. | ✅ | 2 |
| E4. Shadow Runtime And Platform Boundary | Linux and Windows can load, inspect, dispatch, and resource-test the output-suppressed runtime without production activation. | ✅ | 3 |
| E5. Musical Layout Construction | The rig gains usable musical layouts beyond the current full-live-rack baseline, beginning with customizable tonewheel organ and synthesizer layouts. | 🟡 | 1, 4, 6 |
| E6. Control-Only Relay And Switching | SMC-Mixer control routing and control-only profile adoption work transactionally with rollback and bounded fan-out. | 🟡 | 4 |
| E7. Performance Engineering And Audible Stability | Parameter-change cost, control fan-out, scheduling, and live audio behavior are measured and improved without hiding residual faults. | 🟡 | 0, 4, acceptance follow-up |
| E8. MIDI Management Triggers | MIDI management events invoke the same validated switching operations without disturbing musical mappings. | ⬜ | 5 |
| E9. Prepared Engines And Graph Deltas | Prepared plugin engines and graph changes can be committed atomically without blocking audio or MIDI. | ⬜ | 6 |
| E10. Echora Portability | The portable rig model, layouts, runtime contracts, and evidence can be integrated into `/c/development/egt/customers/egt/echora`. | ⬜ | cross-project follow-up |
| E11. Windows Certification | The selected Windows adapters pass runtime, MIDI, plugin-host, lifecycle, performance, and cleanup campaigns. | ⬜ | 7 |
| E12. Deployment Promotion And Cleanup | The experimental runtime has a reviewed cutover, rollback, promotion, and legacy cleanup path. | ⬜ | 8 |

## Sprint Boundary

### Sprint S1: Control-Only Technical Closure

Sprint outcome: establish a reproducible control-only relay and characterize the
remaining live audio risk without promoting production changes.

Completed:

- Protected baseline verification and read-only Airstar observability.
- Offline EQ parameter isolation, call-path investigation, and upstream LSP comparison.
- Per-control MIDI and PipeWire timing instrumentation with offline regression coverage.
- Airstar preflight, guarded relay cutover, all-eight-fader exercise, rollback, cleanup, and final 30/30 validation.
- Zero relay adapter failures, zero observed PipeWire ERR delta, zero journal xrun matches, and complete CC40-47 value coverage.
- Local JSON/JACK suite and the full local 66-test non-JSON suite pass.

Accepted with follow-up:

- The operator reported a very subtle audible issue with few occurrences, materially less than prior equivalent tests.
- The strict audible-stability gate remains open.
- The upstream LSP candidate is offline evidence only and was not installed or promoted.

Sprint close: no production promotion, preset replacement, plugin replacement,
service enablement, or permanent graph change.

### Sprint S2: First Musical Layouts

This is now the active product sprint. Its first contract slice is complete and
the engine-parameter binding and live activation slices remain open. It restores
the original product balance: layouts are deliverables, not only future schema
examples.

Completed first slice:

- Added additive `tonewheel-organ` and `synth-programmer` Arturia Device Profiles.
- Reused the existing Hammond organ and Optik synth engines and verified Arturia hardware preset.
- Added five-slot Rig Profiles that preserve SMK-25, SMC-Mixer, SMC-PAD, and Pocket roles.
- Added deterministic compile checks for both layouts with empty graph deltas.
- Preserved `full-live-rack` as default and fallback; no live graph or plugin was changed.

Candidate backlog:

- Define the `tonewheel-organ` Device Profile and Arturia drawbar layout.
- Define the `synth-programmer` Device Profile and Arturia/keyboard control layout.
- Select and document the initial organ and synthesizer plugin engines using prepared-resource contracts.
- Add deterministic compiler fixtures, ownership rules, mappings, readiness checks, and output-suppressed dry-runs for both layouts.
- Keep the current full-live-rack layout unchanged and prove switch-back behavior in offline tests.
- Specify the first operator acceptance session for organ registration, synth patch shaping, and rollback.

Remaining S2 slices:

- Select genuinely controllable organ and synth engines; the current protected SF2 players expose only generic reverb, chorus, polyphony, and interpolation controls.
- Engine candidates are selected in [S2-ENGINE-SELECTION.md](../../tools/music-rig/S2-ENGINE-SELECTION.md): setBfree for organ and Surge XT for synth.
- Bind semantic organ and synth parameters to verified plugin controls or explicitly mark them unavailable.
- Confirm actual engine control ranges, drawbar inversion, pickup behavior, and state persistence.
- Add output-suppressed runtime dry-runs for both layouts and prove temporary commit/switch-back leaves `full-live-rack` recoverable; prepared engine resource rollback remains open.
- Defer live audio acceptance and Echora integration until these contracts are reviewed.

Parked but not forgotten:

- Compare a protected-plugin legacy route and relay route under an explicitly matched operator workload.
- Attribute the remaining audible occurrences to mixer control, plugin processing, host scheduling, or another graph participant.
- Close the audible gate only when the agreed zero-error and audible criteria pass.

## Epic Exit Rules

- An epic is not complete because its code exists; its stated outcome and evidence gate must pass.
- A live failure remains visible even when a mitigation reduces its frequency.
- Every live sprint names its preflight, operator cue, rollback, cleanup, and final validation.
- Experimental artifacts remain temporary and output-disabled unless the sprint explicitly authorizes a guarded live boundary.
- Promotion is a separate decision from technical evidence and requires explicit approval.

## Planning Order

1. Build the first organ and synthesizer layouts in E5 while preserving full-live-rack.
2. Continue E7 performance work as a quality track without making it the only product output.
3. Start E8 only after the control-only switching contract remains stable.
4. Enter E9 only after prepared-resource budgets, graph-delta semantics, and rollback ownership are reviewed.
5. Port stable contracts and at least one layout through E10 in Echora.
6. Run E11 after the portable and Linux boundaries stop changing.
7. Perform E12 only after all prior epic exit gates and legacy restoration evidence pass.
