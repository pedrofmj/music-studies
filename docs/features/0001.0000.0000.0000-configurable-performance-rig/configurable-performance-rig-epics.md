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
| E5. Control-Only Relay And Switching | SMC-Mixer control routing and control-only profile adoption work transactionally with rollback and bounded fan-out. | 🟡 | 4 |
| E6. EQ Performance And Audible Stability | Parameter-change cost is understood and reduced while live audio remains clean and musically acceptable. | 🟡 | 4, acceptance follow-up |
| E7. MIDI Management Triggers | MIDI management events invoke the same validated switching operations without disturbing musical mappings. | ⬜ | 5 |
| E8. Prepared Engines And Graph Deltas | Prepared plugin engines and graph changes can be committed atomically without blocking audio or MIDI. | ⬜ | 6 |
| E9. Windows Certification | The selected Windows adapters pass runtime, MIDI, plugin-host, lifecycle, performance, and cleanup campaigns. | ⬜ | 7 |
| E10. Deployment Promotion And Cleanup | The experimental runtime has a reviewed cutover, rollback, promotion, and legacy cleanup path. | ⬜ | 8 |

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

### Sprint S2: Residual Audible-Fault Attribution

This sprint is planned, not started. It should begin with a short planning review
and must not assume that the relay technical pass proves audio acceptance.

Candidate backlog:

- Compare a protected-plugin legacy route and relay route under an explicitly matched operator workload.
- Attribute the remaining audible occurrences to mixer control, plugin processing, host scheduling, or another graph participant.
- Define a repeatable operator audio-assessment form and capture its result beside the machine evidence.
- Review the upstream EQ candidate as a separately staged experiment only if its response and packaging boundaries are accepted.
- Close the audible gate only when the agreed zero-error and audible criteria pass; otherwise retain the epic as active.

## Epic Exit Rules

- An epic is not complete because its code exists; its stated outcome and evidence gate must pass.
- A live failure remains visible even when a mitigation reduces its frequency.
- Every live sprint names its preflight, operator cue, rollback, cleanup, and final validation.
- Experimental artifacts remain temporary and output-disabled unless the sprint explicitly authorizes a guarded live boundary.
- Promotion is a separate decision from technical evidence and requires explicit approval.

## Planning Order

1. Close or explicitly defer E6's audible-stability gate.
2. Start E7 only after the control-only switching contract remains stable.
3. Enter E8 only after prepared-resource budgets, graph-delta semantics, and rollback ownership are reviewed.
4. Run E9 after the portable and Linux boundaries stop changing.
5. Perform E10 only after all prior epic exit gates and legacy restoration evidence pass.
