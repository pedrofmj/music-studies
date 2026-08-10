# ADR 0001: Defer Native PipeWire Graph Control

Status: **Accepted**

Date: 2026-08-10

Feature: `0001.0000.0000.0000`

## Decision

Native PipeWire graph mutation is deferred to Milestone 6. Milestones 1 through
5 must not require the new runtime to create or destroy profile-specific
PipeWire links, create or destroy prepared engines or external graph objects,
write PipeWire metadata, change the graph quantum, or invoke a full Patchbay
restore as part of a profile switch.

A switch is `control-only` only when its compiled external and plugin-host graph
delta is empty:

```text
created links = 0
removed links = 0
created engines or graph objects = 0
removed engines or graph objects = 0
metadata changes = 0
```

Fixed runtime-owned JACK/MIDI ports are not a profile-specific graph delta. An
adapter may register those stable ports once for its process lifetime, beginning
with opt-in shadow mode, and an explicit deployment step may provision their
fixed links. Profile selection cannot create, destroy, rename, reconnect, or
otherwise vary those ports or links.

If any value is nonzero, the compiler and runtime must classify the switch as a
prepared graph transaction or a cold operation. That switch cannot be activated
by the Milestone 4 control-only CLI path or by a Milestone 5 MIDI management
trigger.

## Context

The protected rack already has the Carla project, plugins, services, and
PipeWire links needed for its current behavior. Early Device Profiles can change
semantic control mappings and targets while reusing those loaded engines and
stable links. Later shadow and cutover stages can add a fixed set of stable
runtime ports without giving individual profile transactions graph ownership.

Introducing a native graph writer earlier would expand the safety and rollback
surface before the profile model, deterministic compiler, immutable generation
publication, CLI transaction, and MIDI trigger queue have independent parity.
It would also duplicate ownership with the protected Patchbay service while the
legacy deployment remains the production authority.

The deferral does not remove graph semantics from the design. Schemas and the
compiler still model and calculate graph requirements so an unsupported delta
is rejected before activation rather than ignored.

## Milestone Boundary

### Milestone 1: Definitions

Schemas model semantic endpoints, ownership, readiness, and graph requirements.
They do not mutate a live graph.

### Milestone 2: Compilation

The compiler calculates deterministic graph deltas and may materialize output
only in temporary test locations. It proves parity against the protected
49-plugin, 111-project-connection, and 115-link authorities without applying
the generated result.

### Milestone 3: Shadow Runtime

The runtime may observe endpoint readiness through a read-only adapter. Shadow
mode suppresses all outputs and leaves the legacy services as behavior owners.
After explicit opt-in, an adapter may register stable input ports and a separate
deployment step may provision fixed duplicated-input links. Neither action is a
profile switch, and the ports and links remain invariant across profiles.

### Milestone 4: Control-Only Switching

Only immutable mapping/state generations with an empty graph delta are
eligible. A profile that needs another link, engine, port, or metadata value is
rejected as not control-only.

The first SMC-Mixer `eight-band-eq` and `multilevel-volume` profiles may proceed
only if both resolve to already running targets through the same protected
graph. This decision does not assume that parity; compilation and readiness
tests must prove it.

### Milestone 5: MIDI Management Triggers

MIDI triggers reuse the same transaction API and eligibility result as the CLI.
A trigger cannot bypass graph-delta classification or enable a graph-changing
switch.

### Milestone 6: Prepared Engines And Graph Deltas

Milestone 6 may introduce native PipeWire mutation behind the audio/graph
adapter after staging, verification, ramp/crossfade, rollback, and benchmark
requirements are implemented.

## Allowed Before Milestone 6

- Read-only endpoint and graph discovery.
- Read-only comparison with compiled requirements.
- Deterministic graph-delta calculation.
- Temporary test materialization outside protected paths.
- Reporting that a target is not ready or not control-only.
- Process-lifetime registration of fixed runtime-owned ports from opt-in shadow
  mode onward.
- Explicit, profile-independent provisioning of their fixed deployment links.
- Continued ownership by the protected Carla project and Patchbay service.

## Prohibited Before Milestone 6

- Creating, destroying, or reconnecting profile-specific live PipeWire links
  during a profile switch.
- Creating or destroying prepared engines, third-party nodes, or
  profile-specific ports during a profile switch.
- Writing PipeWire metadata or changing the production quantum.
- Calling the protected Patchbay restore as part of profile activation.
- Treating a nonempty or unknown graph delta as control-only.
- Falling back to a partial switch when graph capability is unavailable.

## Milestone 6 Entry Conditions

Native graph control work may start only when:

1. the protected baseline and preview-first restore path still pass;
2. current profiles compile deterministically with semantic parity;
3. CLI and MIDI requests share one tested transaction API;
4. graph capabilities are explicit in platform bindings;
5. staged changes can be verified before publication;
6. full rollback restores the prior graph and active generation;
7. ramps or crossfades cover audible topology changes; and
8. benchmark evidence records commit, adoption, xruns, dropouts, CPU, and
   separated memory.

Production restore remains an operator recovery action. It is not the normal
rollback mechanism for a profile transaction.

## Consequences

### Benefits

- Milestones 1 through 5 remain testable without touching the production graph.
- Control-only switching has a smaller, deterministic transaction surface.
- CLI and MIDI profile selection cannot silently gain different graph powers.
- Linux PipeWire details do not shape the portable core or Windows contracts.
- The protected rack remains immediately usable throughout early development.

### Costs

- Profiles that require new instruments, routes, or prepared engines wait for
  Milestone 6 even if their semantic definitions exist earlier.
- Early profile choices are constrained to the currently loaded graph.
- Readiness and compilation must distinguish mapping changes from graph changes
  precisely.

## Enforcement

- The portable core owns switch classification but no PipeWire primitive.
- The existing `music_rig_core_portability` test rejects platform backend
  tokens; it is extended with explicit PipeWire and Carla name guards.
- Milestone 2 tests must reject a control-only result with a nonempty graph
  delta.
- Milestone 3 shadow tests must prove output suppression.
- Milestone 4 and 5 acceptance requires zero graph or service changes caused by
  profile selection.

## Reconsideration Triggers

Supersede this decision before Milestone 6 only if an early required Device
Profile cannot be represented with the protected loaded graph and postponing
that profile blocks the agreed migration sequence. The replacement decision
must preserve the production default, explicit opt-in activation, complete
rollback, and the same cross-platform transaction semantics.
