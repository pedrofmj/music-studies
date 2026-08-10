# ADR 0002: Carla Control And Prepared-Engine Boundary

Status: **Accepted**

Date: 2026-08-10

Feature: `0001.0000.0000.0000`

## Context

The protected Airstar setup runs Carla `2.5.10` as a Flatpak and loads the
captured `pedro.uproject`. The protected project has a fixed checksum, 49
uniquely named plugins, and 111 project connections. It is the production
authority until the configurable runtime completes its cutover gates.

Early Device Profiles need to change behavior without restarting Carla,
loading plugins, or rebuilding the audio graph. Later profiles may need an
organ, synthesizer, choir, piano, or another large engine whose assets and
plugin state cannot be loaded inside the switch latency budget.

Carla exposes several different kinds of operations. Treating all of them as
equally suitable for a live switch would hide blocking work, weak readiness
signals, and rollback risk behind one adapter method.

## Source Evidence

The decision is evaluated against the captured Carla `2.5.10` deployment and
the upstream API for that version:

- the [Carla 2.5.10 release](https://github.com/falkTX/Carla/releases/tag/v2.5.10);
- the [Carla 2.5.10 host API](https://raw.githubusercontent.com/falkTX/Carla/v2.5.10/source/backend/CarlaHost.h);
- the [official Carla manual](https://kx.studio/Documentation%3AManual%3ACarla);
- `docs/tools/airstar-live-setup/setup.json`; and
- `src/audio-software/carla/projects/pedro-live-rack/project.json`.

The upstream host API provides parameter and internal-control setters, plugin
activation, program selection, plugin lifecycle and state operations, project
loading, patchbay operations, callbacks, runtime information, and last-error
reporting. The manual documents MIDI CC automation and OSC remote control.

Two constraints follow directly from the API:

1. parameter and activation setters return no success value, so a call alone
   is not proof that the requested state became audible; and
2. project loading does not first remove existing plugins. A caller that wants
   replacement semantics must remove them explicitly.

The public host API is an in-process embedding API: its caller owns a host
handle, initializes an engine, and services that engine regularly. Therefore,
it is not assumed to be a client API for the already running protected Carla
GUI process. This is an architectural inference from the documented lifecycle,
not a claim that Carla cannot be controlled remotely.

OSC is a candidate transport, but the exact commands, acknowledgement model,
readback behavior, and Flatpak access of the captured standalone deployment
have not been proven. They must be tested against an isolated Carla instance
before an OSC adapter is selected.

## Decision

### 1. Carla Stays Outside The Portable Core

In this system, the captured Carla deployment is a Linux plugin-host binding.
Authored Rig and Device Profiles express semantic capabilities such as
`organ.drawbar.1`, `piano.lid`, or `mixer.band.3.gain`; they do not contain
Carla plugin numbers, parameter indices, OSC paths, process identifiers, or
project filenames.

The compiled platform binding resolves a semantic target to a plugin instance
and parameter. Windows may use a different host while preserving the same
profile and CLI contracts.

### 2. Early Switches Reuse The Prepared Protected Rack

Through Milestone 5, the protected Carla project is started and owned by the
existing deployment. The new runtime may observe it in read-only or
output-suppressed shadow mode. A control-only switch may later use the existing
MIDI/CC routes or a proven direct parameter transport, but only against plugins
that are already loaded and ready.

No pre-Milestone-6 switch may:

- start, stop, or restart Carla;
- load or replace a Carla project;
- add, remove, replace, or clone a plugin;
- load plugin state or sample assets;
- change Carla or external patchbay topology; or
- wait for plugin discovery, disk I/O, or engine initialization.

The first SMC-Mixer profiles therefore change compiled control mappings for the
existing rack. They do not restructure it.

### 3. Operations Have Explicit Classes

| Carla-related operation | Earliest class | Rule |
| --- | --- | --- |
| Read plugin identity, ports, parameters, ranges, or runtime status | Observe | Read-only; never evidence of audible readiness by itself |
| Send existing MIDI CC automation | Control-only candidate | Target must be loaded; mapping and resulting state must be verified |
| Set a plugin parameter or Carla internal volume, dry/wet, balance, or pan | Control-only candidate | Requires an isolated transport proof, bounded call, callback or readback verification, and rollback value |
| Select a program or MIDI program | Prepared until proven | Plugin-specific latency, asset behavior, audible transition, and rollback must be certified before reclassification |
| Activate or deactivate a plugin | Prepared until proven | Must not expose an unready engine or create an audible discontinuity |
| Load plugin state or a state chunk | Prepared | Complete before commit; verify readiness and expected fingerprint |
| Add, remove, replace, or clone a plugin | Prepared | Perform outside the switch path and inside the resource budget |
| Connect or disconnect Carla patchbay ports | Prepared, Milestone 6 | Stage and verify with the complete graph delta |
| Load a Carla project | Cold management only | Forbidden in a normal live switch transaction |
| Discover, install, or scan plugins and load uncached sample assets | Cold management only | Allowed only through explicit non-live preparation |

An operation is not promoted merely because its API call returns quickly. Its
whole path, including acknowledgement, readiness, audible adoption, and
rollback, must satisfy the relevant benchmark and stability gates.

### 4. Preparation And Commit Are Separate

Milestone 6 introduces the plugin-host adapter and prepared-engine lifecycle:

1. resolve and fingerprint the target plugin, assets, state, and parameters;
2. allocate resources and load all engines outside the commit path;
3. keep new audio paths silent while checking readiness;
4. stage the Carla and external graph delta;
5. verify the staged topology and capture rollback controls;
6. commit only prevalidated control values, generations, and gain ramps; and
7. report adoption only after callback or readback evidence.

Project reload is not the preparation primitive. Prepared instances are
managed explicitly so that ownership, memory, readiness, and rollback remain
observable.

### 5. Identity And Verification Are Mandatory

A binding identifies an instance with stable semantic metadata and a compiled
fingerprint. A transient plugin number or display name alone is insufficient.
The fingerprint must cover the host/plugin format identity, plugin URI or
binary identity as applicable, parameter symbol or validated index, authored
mapping version, and required state/assets.

Before a write, the adapter confirms that the observed instance and parameter
range match the compiled binding. After a write, it confirms adoption through
a supported callback or readback path. A void setter is never treated as an
acknowledgement.

Timeout, identity drift, missing readback, lost host connection, or readiness
failure leaves the previous generation authoritative. Rollback restores the
captured controls and gain state; it does not reload the entire project.

### 6. Transport Selection Requires An Isolated Proof

The Linux adapter may select OSC, an in-process owned Carla engine, or another
documented local control transport only after an isolated proof records:

- compatibility with the deployed Carla version and Flatpak boundary;
- the exact supported operations and message framing;
- request acknowledgement or deterministic state readback;
- disconnection, timeout, and stale-instance behavior;
- bounded control-thread latency with no real-time callback blocking;
- plugin readiness evidence; and
- rollback after partial failure.

Until that proof exists, the supported early boundary is the protected rack's
existing MIDI/CC control path. Direct Carla control is not a Milestone 1-5
dependency.

## Safety And Compatibility Consequences

- The protected project, launcher, services, and saved Patchbay graph remain
  unchanged and continue to be the production default.
- Tests use parsed project fixtures, fake transports, or isolated Carla
  instances. They never discover capabilities by writing to the live rack.
- Control callbacks never perform file I/O, plugin lifecycle work, graph
  mutation, allocation, or synchronous preparation.
- A host-specific parameter that lacks a portable semantic capability remains
  in the platform binding and cannot leak into a Device Profile.
- A future MIDI profile trigger invokes the same compiled transaction as the
  CLI; it does not send ad hoc Carla commands.

## Rejected Alternatives

### Reload The Project For Every Profile

Rejected because project loading is not replacement-transactional, includes
unbounded engine and asset work, and cannot satisfy fast rollback.

### Put Carla IDs In Authored Profiles

Rejected because plugin order, host choice, and parameter indexing are
deployment details and would make profiles non-portable.

### Assume Every Setter Is Control-Only

Rejected because call return, engine adoption, audible readiness, and rollback
are different events. Some plugins defer work or load assets after a control
change.

### Require Direct Carla Control Before Schema Work

Rejected because the current rack already exposes the controls needed for the
first profile extractions. The portable semantic contract can be implemented
and tested without taking ownership of the protected host.

## Follow-Up Gates

- Milestone 1 records semantic plugin capabilities and readiness requirements
  without Carla identifiers in portable profile documents.
- Milestone 2 materializes Carla output only into temporary test locations and
  proves parity with the protected project.
- Milestone 3 observes the protected host in output-suppressed shadow mode.
- Milestones 4 and 5 use only certified control-only mappings.
- Milestone 6 selects and implements the isolated-proven Carla transport,
  prepared-engine lifecycle, staged graph delta, gain transition, and rollback.
- Milestone 7 certifies equivalent behavior through the selected Windows
  plugin-host adapter.

No live Carla process, project, service, or graph connection was changed while
recording this decision.
