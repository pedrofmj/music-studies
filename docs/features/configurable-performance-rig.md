# Feature Proposal: Configurable Performance Rig

Language: **English** | [Português (Brasil)](configurable-performance-rig.pt-BR.md)

Implementation: [Implementation Plan](configurable-performance-rig-implementation-plan.md)

Status: proposed for review; no implementation is implied by this document.

## Summary

The complete transferable music system will be called the **Performance Rig**,
or **Rig** when the context is clear. A Rig includes the controllers, sound
engines, effects, mappings, routes, assets, services, machine-independent
settings, and the rules required to materialize and operate them.

The current Pedro Carla live rack is one fixed configuration of that Rig. This
feature generalizes it into two selectable levels:

- A **Rig Profile** is a global configuration of the whole Rig. It selects the
  participating Device Profiles, sound modules, shared processing, routes, and
  initial state.
- A **Device Profile** gives one device slot a particular musical or operational
  role. The SMC-Mixer can be an equalizer in one profile and a track mixer in
  another, for example.

Both levels must be switchable from a CLI. The runtime design must also allow
MIDI CC, note, or program-change events to invoke the same switching operations
later. Switching must favor predictable low latency and low idle resource use.

The same authored Rig and Profile definitions must be portable across supported
operating systems. Platform-specific audio, MIDI, graph, plugin-host, IPC,
filesystem, and service-lifecycle behavior belongs behind adapters rather than
inside the musical profile model.

## Why This Is Needed

The repository now preserves and transfers the complete current setup between
machines, but roles are embedded in a monolithic Carla project, PipeWire graph,
services, and `setup.json` manifest. The installation is reproducible, but its
musical configuration is effectively static.

Examples of the current coupling include:

- the Arturia is always a nine-instrument rack controller;
- the SMC-Mixer is always an eight-band equalizer;
- the SMC-PAD and SMC-PAD Pocket always play the same drum instrument; and
- the SMK-25 always controls eight latched pad layers.

Changing one role currently means editing and recapturing the whole rack. The
desired model keeps the reproducibility of the existing setup while allowing a
device role, or the entire Rig configuration, to be selected deliberately.

## Terminology

### Performance Rig

The durable concept for the complete system. The Rig is not a particular
computer and is not a single Carla project. It is the portable definition from
which a working setup can be installed and run on a compatible machine.

Suggested identifier for this repository's first Rig:
`pedro-performance-rig`.

### Rig Profile

A named, versioned, globally selectable composition within a Rig. A Rig Profile
chooses Device Profiles and the shared audio/MIDI graph needed for a particular
performance context. The existing setup would initially become a Rig Profile
such as `full-live-rack` with no behavioral changes.

"Global profile" is acceptable CLI and user-facing shorthand for Rig Profile.

### Device Slot

A stable logical position in a Rig, such as `arturia-main`, `smc-mixer-main`,
`smc-pad-main`, or `smc-pad-pocket`. A slot is distinct from a device model and
from a transient ALSA or PipeWire port name. This matters because several
M-VAVE devices share a USB product ID and similar SINCO endpoints.

### Device Profile

A named, versioned behavior assigned to one device slot. It defines:

- compatible device models and required endpoints;
- the semantic purpose of each participating physical control;
- accepted MIDI messages and hardware-mode assumptions;
- transformations, actions, parameter targets, and feedback;
- owned sound engines, effects, routes, or helper logic;
- default state and state-persistence rules;
- resource dependencies and preparation policy; and
- switch-in, switch-out, failure, and rollback behavior.

A Device Profile describes the role of the device in the Rig, not only the raw
MIDI messages emitted by its controls.

### Hardware Preset

Settings stored inside a controller or applied with MIDI Control Center or
CubeSuite. These settings determine which raw messages the hardware emits.

The existing `controller-profiles.md` uses "profile" for this concept. During
implementation it should be renamed or reframed as **Hardware Presets** so it
cannot be confused with Device Profiles. A Device Profile references a
compatible Hardware Preset instead of duplicating it.

### Switch Trigger

A CLI request or MIDI event that asks the Rig runtime to activate a Rig Profile
or Device Profile. Every trigger source uses the same validation and switching
engine.

## Conceptual Model

```text
Performance Rig
  +-- device slots and physical-device discovery rules
  +-- available Device Profiles per slot
  +-- available Rig Profiles
  |     +-- one selected Device Profile per participating slot
  |     +-- shared modules, routes, and initial state
  +-- Hardware Presets
  +-- switch triggers and safety policy
  +-- deployment/runtime requirements
```

The Rig is the container. A Rig Profile changes the global composition. A
Device Profile switch changes only one slot and its owned dependencies while
preserving the rest of the active Rig Profile.

Only one Device Profile is active per slot in the first implementation. A
future profile may be composed from reusable behavior fragments, but fragments
are not required for the initial design.

## Profile Examples

The names below are illustrative identifiers, not final preset names.

### M-VAVE SMC-Mixer

#### `eight-band-eq`

This is the current behavior. Faders control 63, 125, 250, 500, 1000, 2000,
4000, and 8000 Hz bands through bounded gain mappings. The profile owns the CC
transformations and equalizer parameter bindings.

#### `multilevel-volume`

The surface controls several levels of the live mix rather than frequency
bands. Possible banks include individual instruments, instrument families,
submixes, effects returns, monitor level, and master level. Encoders can control
pan or effects sends while buttons select banks or mute groups.

#### `track-control`

The controller uses its intended DAW-surface role: faders control track volume,
encoders control pan, buttons control mute/solo/record/select, and navigation
changes track banks. This profile may use Mackie Control rather than the current
plain CC Hardware Preset.

#### `effects-mixer`

Faders control effect returns or wet/dry balances, encoders control send levels
or core effect parameters, and buttons bypass or select effect groups. This is
useful when the main instrument mix is owned by another device.

### M-VAVE SMC-PAD And SMC-PAD Pocket

The two units occupy independent slots. They may use the same profile, different
profiles, or only one may participate in a Rig Profile.

#### `drum-set`

This is the current note-based behavior. Pads play a drum SoundFont; knobs
control instrument volume and post-instrument gain where available.

#### `pad-layer-controller`

Pads activate pad tracks or instruments in the style of the current SMK-25 pad
layers, adapted to a pad-first surface. A pad can use momentary, latch, toggle,
or one-shot behavior. Encoder banks can control layer volume, filter, reverb,
or other layer parameters.

One example is to use the full SMC-PAD for 16 independently playable layers and
the Pocket for drum sounds. Another is to reserve the full unit for drums and
use the Pocket to toggle four or eight ambient layers.

#### `clip-and-scene-launcher`

Pads launch clips, sections, backing tracks, loops, or song scenes. Colors and
aftertouch can provide state or expressive feedback where the hardware and
runtime support it. Transport controls own stop, play, record, and tempo actions.

#### `melodic-grid`

Pads form a chromatic, scale-locked, chord, or isomorphic note layout for a
synthesizer or sampler. Banks change octave, scale, or instrument.

#### `percussion-zones`

Pad groups target different percussion engines, such as acoustic drums,
electronic hits, orchestral percussion, and one-shot effects. Encoders control
the level or sound shape of each group.

### Arturia KeyLab Essential 61 mk3

#### `multi-instrument-rack`

This is the current behavior. Keys play the rack, faders control nine instrument
levels, knobs control reverb sends, and the central encoder/click controls rack
volume and mute.

#### `tonewheel-organ`

The Arturia becomes a detailed organ controller. Nine faders act as drawbars;
knobs control parameters such as percussion, key click, leakage, chorus/vibrato,
rotary speed, drive, or microphone balance. Pads can select registrations or
switch the rotary state. Drawbar direction and ranges must be explicit because
organ drawbars use an inverted physical metaphor.

#### `synth-programmer`

Keys play one synthesizer while faders and knobs shape it. Assignments can cover
oscillator mix, envelopes, filter cutoff/resonance, modulation depth/rate,
unison, drive, and effects. Pads can choose oscillator states, modulation
routes, sequences, or saved variations.

#### `choir-designer`

Keys play a choir engine. Controls shape section balance, vowel or formant,
attack/release, dynamics, expression, divisi, stereo width, room, and reverb.
Pads can select articulations or choir combinations.

#### `modeled-piano`

Keys play a detailed piano engine. Knobs and faders expose parameters supported
by that engine, including lid opening (`abertura da caixa`), string tension or
tuning (`tensao`), hammer hardness, velocity response, sympathetic resonance,
damper and key-off noise, microphone position, room, and reverb. The profile
must advertise engine-specific compatibility rather than assuming every piano
plugin provides every parameter.

#### `split-orchestra`

The keyboard is divided into zones for piano, strings, brass, choir, bass, or
lead sounds. Faders mix zones; knobs control expression or effects; pads select
articulations and variations.

#### `daw-and-looper-control`

The standard MIDI port remains available for keys while the MCU/HUI port and
transport surface control recording, track selection, looping, and playback.
This profile must keep DAW-control messages isolated from instrument mappings.

### M-VAVE SMK-25

The current `ambient-pad-layers` profile can be joined by alternatives such as
`split-instrument`, `chord-and-arpeggio`, `clip-launcher`, or a compact
`instrument-parameter-editor`. These examples verify that Device Profiles are
a shared abstraction rather than special cases for only three products.

## CLI Requirements

The proposed executable name is `music-rig`. It avoids coupling the interface
to Carla, PipeWire, a reference hostname, or one person's current project name.

The minimum command surface is:

```bash
# Inspect definitions and runtime state.
music-rig status
music-rig status --json
music-rig profiles list
music-rig profiles list --device smc-mixer-main

# Switch the complete Rig composition.
music-rig switch --global full-live-rack

# Switch only one device slot.
music-rig switch \
  --device smc-mixer-main \
  --profile multilevel-volume

# Check or stage a change without committing it.
music-rig switch --global modeled-piano --dry-run
music-rig prepare --global modeled-piano
music-rig validate
```

CLI behavior:

- `--global` and `--device` are mutually exclusive switch scopes.
- A device switch changes only the requested slot and resources owned solely by
  its previous or next profile.
- A global switch is transactional: all constituent profiles activate or none
  do.
- `--dry-run` reports compatibility, graph changes, missing assets, resource
  readiness, and whether a cold load would be required.
- `prepare` loads or validates expensive dependencies without making them
  audible or changing control ownership.
- Normal live switching fails quickly if a required profile is cold. An
  explicit non-live option may allow a blocking cold load.
- Commands return only after the new generation is committed or the previous
  generation has been preserved. Machine-readable output and stable exit codes
  are required for scripts and future user interfaces.
- `status` identifies the active Rig Profile, per-device overrides, profile
  readiness, resource use, and the last switch result.

The CLI is a thin client. It must communicate with one resident user service
over a local Unix socket or equivalent low-overhead IPC. It must not rebuild a
project, launch a shell pipeline, or start a new MIDI/audio engine on every
switch.

## MIDI Switch Triggers

MIDI-triggered switching is a planned capability and must shape the first data
model even if it is implemented after the CLI.

A trigger binds a fully qualified event to the same operation exposed by the
CLI. Example intent:

```json
{
  "source_slot": "arturia-main",
  "endpoint": "standard-midi",
  "event": { "type": "cc", "channel": 16, "number": 24 },
  "edge": "value-at-least",
  "threshold": 64,
  "consume": true,
  "action": {
    "type": "switch-device-profile",
    "device_slot": "smc-mixer-main",
    "profile": "multilevel-volume"
  }
}
```

Required trigger behavior:

- CC, note, and program-change messages are supported by the model.
- Endpoint, channel, event type, number, and value/edge are matched explicitly.
- Press and release pairs are debounced so one pad press cannot switch twice.
- A trigger declares whether its event is consumed or also reaches the active
  musical profile.
- Management triggers live in a persistent Rig control layer, independent of
  the currently active musical Device Profile. Switching the Arturia's musical
  role must not accidentally remove the only route used to switch back.
- Trigger conflicts and recursive switch loops are rejected during validation.
- In live mode, MIDI triggers may activate only profiles that have passed
  validation and meet their readiness policy. A heavy cold load must not block
  the MIDI event-processing path.
- MIDI handling calls the switching service internally. It never spawns the CLI
  executable per event.

Example Arturia pad uses include selecting complete Rig Profiles, selecting an
organ registration profile, changing only the SMC-Mixer from EQ to track
control, or cycling the SMC-PAD between drums and layer control. LED or display
feedback is optional initially but belongs in the profile model.

## Runtime Architecture

### Resident Control Service

A single per-user service owns active-profile state, compiled MIDI mappings,
switch transactions, and coordination with audio/MIDI adapters. It should be
event-driven and dormant when there is no command or MIDI input.

The service separates a non-real-time control plane from audio and MIDI event
paths. JSON parsing, dependency discovery, plugin loading, graph planning, and
filesystem work occur before commit and never inside a real-time callback.

### Compiled Profile Generation

At install, validation, or preparation time, declarative profiles are resolved
into immutable runtime mappings and a graph delta. Activation swaps one
prevalidated generation for another rather than reparsing manifests or
recapturing the entire PipeWire graph.

Control-only changes should reduce to an atomic mapping-generation change.
Graph changes should apply the smallest required delta and preserve shared
engines, effects, and routes.

### Transactional Switching

A switch follows these stages:

1. Resolve the requested global or device profile and its dependencies.
2. Validate compatibility, ownership, readiness, and resource budget.
3. Prepare an immutable next generation outside real-time processing.
4. Apply any required short mute/ramp or crossfade.
5. Commit mappings and the minimal graph delta atomically.
6. Publish state and release resources no longer needed.

Any failure before commit leaves the active generation untouched. Any failure
during commit rolls back to the previous complete generation.

### State And Physical Controls

Profile state belongs to a qualified address containing the Rig, device slot,
profile, and parameter. Two profiles must not accidentally share a raw CC state
just because they use the same number.

Each absolute control declares a switch policy such as `jump`, `pickup`,
`scaled-pickup`, or `ignore-until-moved`. Safe defaults should prevent an old
fader position from causing a sudden volume or parameter jump after switching.
Relative encoders declare their encoding and acceleration rules.

Profiles also declare whether their last values persist across activation,
reset to authored defaults, or follow values stored by the controlled engine.

## Performance And Resource Requirements

Fast switching and low resource use can conflict when a new profile requires a
large SoundFont, sampler library, or plugin. The design must make this tradeoff
explicit instead of claiming that every cold profile can switch instantly.

Profiles use one of these readiness classes:

- **Control-only:** changes mappings or targets while reusing running engines.
  It should be immediately switchable.
- **Prepared:** requires engines or assets that have been loaded in advance but
  are silent or disconnected until commit.
- **Cold:** requires plugin or asset loading. It is suitable for setup time or
  an explicitly blocking switch, not an unannounced live MIDI trigger.

Initial performance targets on the documented `airstar` reference machine are:

- control-only switch: p95 at or below 20 ms from service receipt to commit;
- prepared audio/profile switch: p95 at or below 100 ms, including a short
  click-free ramp or crossfade;
- MIDI control trigger to control-only commit: p95 at or below 30 ms;
- idle switching-service CPU: below 0.5% of one core when no events arrive;
- switching-service resident memory: below 50 MB, excluding plugin engines,
  sample assets, Carla, and PipeWire; and
- no audio xruns attributable to profile activation in the repeatable switch
  benchmark.

These are acceptance targets to benchmark and tune, not claims about the
current implementation. They may be tightened after a baseline is measured.

Resource rules:

- Do not poll when ALSA/PipeWire notifications or socket events are available.
- Do not duplicate shared instruments or effects between profiles unnecessarily.
- Keep only explicitly prepared profiles warm; support a configurable memory
  ceiling and deterministic eviction of unpinned prepared profiles.
- Load assets and instantiate plugins off the real-time path.
- Cache validated definitions and compiled mappings, invalidating them only when
  their source files or dependencies change.
- Apply graph deltas instead of restarting Carla or restoring the entire saved
  graph for a one-device change.
- Reuse one MIDI input subscription per physical endpoint and dispatch through
  the active immutable mapping generation.
- Measure activation latency, xruns, CPU, and memory in automated benchmarks.

The preparation policy is part of each Rig Profile. A performance profile can
pin an organ and piano for instant switching, while a resource-constrained
practice profile can keep only the active engine resident.

## Declarative Ownership And Validation

Every profile must declare what it owns. Owned objects can include MIDI events,
semantic controls, parameter targets, helper processors, plugins, graph ports,
routes, state keys, and feedback outputs.

Validation rejects:

- two active profiles claiming the same exclusive control or parameter;
- ambiguous device discovery or a missing required endpoint;
- a profile incompatible with the slot's bound model or Hardware Preset;
- missing plugins, assets, helper capabilities, or unsupported parameters;
- private transformed CC collisions;
- MIDI management triggers shadowed by musical mappings;
- global profiles that cannot be committed as one complete generation; and
- a live-only profile whose dependencies cannot satisfy its readiness policy.

Profiles should target semantic parameters where an adapter can provide them,
for example `organ.drawbar.16ft` or `piano.lid`, while adapters resolve those
names to plugin-specific ports, MIDI CCs, or Carla parameters. Raw MIDI and
plugin indices remain allowed at adapter boundaries but should not define the
top-level Rig composition.

## Portability And Multiplatform Requirements

Portability is a product requirement, not only a future refactoring direction.
The first implementation may reach live parity on Linux before another platform,
but schema, core runtime, CLI, IPC protocol, state model, and tests must remain
platform-neutral from their first version.

The initial support targets are:

- **Linux:** the first validated backend, using the existing
  PipeWire/JACK/Carla/systemd deployment;
- **Windows:** the required second supported backend, using Windows-compatible
  MIDI, audio, IPC, lifecycle, and plugin-host adapters; and
- **macOS:** not required for the initial completion gate, but the adapter
  interfaces must not prevent a later CoreMIDI/CoreAudio implementation.

Multiplatform behavior requires:

- the same authored Rig, Rig Profile, Device Profile, Hardware Preset, and
  Switch Trigger documents to be usable without platform-specific copies;
- platform bindings to resolve semantic capabilities to available ports,
  plugins, parameters, graph operations, and filesystem locations;
- no hard-coded PipeWire aliases, Unix paths, systemd units, Windows paths, or
  platform plugin identifiers in the top-level musical composition;
- the same `music-rig` command names, arguments, state semantics, response
  fields, and stable error codes on every supported platform;
- a versioned compiled format and state format that can be inspected and moved
  between supported platforms;
- explicit capability diagnostics when a platform cannot satisfy a profile;
  and
- platform-specific performance measurements against the same benchmark
  definitions, with thresholds recorded per reference machine when necessary.

A platform adapter may substitute a different plugin or graph mechanism only
when it satisfies the same declared semantic capabilities. A silent behavioral
downgrade is invalid. At least one complete Rig Profile must pass installation,
validation, global switching, device switching, state restore, and recovery on
both Linux and Windows before the system is considered multiplatform.

## Proposed Repository Shape

The exact schema remains an implementation decision, but this separation is a
useful starting point:

```text
src/performance-rigs/pedro-performance-rig/
  rig.json
  rig-profiles/
    full-live-rack.json
    modeled-piano.json
  device-profiles/
    arturia-main/
      multi-instrument-rack.json
      tonewheel-organ.json
      synth-programmer.json
    smc-mixer-main/
      eight-band-eq.json
      multilevel-volume.json
      track-control.json
    smc-pad-main/
      drum-set.json
      pad-layer-controller.json
    smc-pad-pocket/
      drum-set.json
      pad-layer-controller.json
  hardware-presets/
  platform-bindings/
    linux/
    windows/
  switch-triggers.json
```

The first extraction may reference the existing Carla project, setup manifest,
Patchbay snapshot, and services rather than moving or rewriting them. Once the
runtime can reproduce the current behavior through the new composition, those
artifacts can be split only where the profile boundaries require it.

## Functional Requirements

1. Install and validate a named Performance Rig independently of hostname and
   operating-system user account under the same portability rules as the
   current setup.
2. List available and active Rig Profiles and Device Profiles.
3. Switch the complete Rig with one global CLI operation.
4. Switch one device slot without resetting unrelated slots.
5. Preserve an explicit per-device override in runtime status until a global
   switch replaces it or the user resets it to the Rig Profile default.
6. Dry-run, prepare, commit, report, and roll back a switch transaction.
7. Reproduce the current full live rack as the first Rig Profile without a
   musical or routing regression.
8. Allow the SMC-PAD and SMC-PAD Pocket to use different profiles concurrently.
9. Represent CLI and MIDI triggers as calls to the same switching operation.
10. Preserve a management MIDI path independently of active musical mappings.
11. Prevent unsafe parameter jumps through declared takeover policies.
12. Report cold dependencies before a time-critical switch is attempted.
13. Compile the same authored definitions for Linux and Windows without copying
    or forking the musical profile documents.
14. Keep CLI commands, transaction semantics, state semantics, and error codes
    consistent across supported platforms.
15. Report platform capabilities and unresolved profile requirements before
    installation or activation.
16. Resolve operating-system paths, device APIs, graph operations, services, and
    plugin implementations through platform bindings.
17. Validate at least one complete Rig Profile on both Linux and Windows.

## Non-Goals For The First Implementation

- A graphical profile editor.
- Automatic inference of a useful Device Profile from raw MIDI traffic.
- Seamless cold loading of arbitrarily large sample libraries.
- Switching profiles directly inside a real-time callback.
- Releasing every operating-system adapter simultaneously. Linux is the first
  live backend, Windows is the required second supported backend, and other
  platforms can follow the same adapter contract.
- Replacing Carla, PipeWire, or the current verified deployment before feature
  parity is demonstrated.
- Composable subprofiles or multiple simultaneous profiles on one slot.

## Delivery Plan

### Phase 1: Preserve Current Behavior

- Introduce Rig, slot, Rig Profile, Device Profile, and Hardware Preset schemas.
- Describe the existing setup as `pedro-performance-rig/full-live-rack`.
- Extract one current Device Profile for every controller slot.
- Compile or materialize those definitions into the existing artifacts.
- Prove parity with current plugin, connection, service, asset, and live checks.

### Phase 2: CLI And Runtime Service

- Add the resident switching service and `music-rig` client.
- Implement status, list, validate, dry-run, prepare, and transactional switch.
- Start with control-only Device Profile switching and benchmark it.
- Add graph-delta ownership and per-device overrides.

### Phase 3: First Alternative Profiles

- Implement one low-risk SMC-Mixer alternative, likely `multilevel-volume`.
- Implement independent `drum-set` and `pad-layer-controller` assignments for
  SMC-PAD and SMC-PAD Pocket.
- Add a focused Arturia sound-design profile after selecting a plugin whose
  parameters can be addressed reliably.

### Phase 4: MIDI Triggers

- Add persistent management mappings for Arturia pads or other controls.
- Route CC/note/program events directly into the switching service.
- Add debounce, conflict validation, readiness enforcement, and feedback.
- Benchmark end-to-end trigger latency and repeated switching under audio load.

### Phase 5: Prepared Audio Switching

- Add background preparation and bounded warm-resource caching.
- Add click-free ramps/crossfades and engine lifecycle coordination.
- Implement and benchmark global switches between materially different Rig
  Profiles such as the full rack, organ, synth, choir, and modeled piano.

### Phase 6: Second-Platform Delivery

- Build the portable core and CLI on Windows using the same schemas and
  compiled contracts.
- Implement Windows adapters for IPC, paths/state, device discovery, MIDI,
  audio/graph control, plugin hosting, and service lifecycle.
- Add platform bindings for one complete Rig Profile.
- Run the same switching, recovery, state, and performance test suites on the
  Linux and Windows reference machines.

## Acceptance Criteria

- The `full-live-rack` Rig Profile validates against and reproduces the current
  checked-in setup.
- A CLI global switch either commits every selected profile or changes nothing.
- A CLI device switch leaves unrelated device mappings and audio routes unchanged.
- The two pad controllers can run different Device Profiles simultaneously.
- CLI and MIDI requests produce the same validated target state.
- A management trigger remains usable after changing the Arturia's musical
  Device Profile.
- A cold profile is reported or prepared without blocking MIDI/audio processing.
- Switch latency, idle CPU, resident memory, and xruns are measured against the
  targets in this document.
- Disconnect, dependency-failure, and mid-switch-error tests preserve or restore
  the previous valid generation.
- Existing machine-transfer installation and validation continue to work.
- One complete Rig Profile installs and passes global switching, device
  switching, state restore, validation, and rollback on both Linux and Windows.
- The same authored musical profile documents and CLI contract are used on both
  platforms; only platform bindings and adapters differ.
- Unsupported capabilities fail explicitly before activation.

## Decisions To Review Before Implementation

1. Confirm **Performance Rig** as the name of the complete setup.
2. Confirm **Rig Profile** for a global profile and **Device Profile** for one
   device role.
3. Confirm **Hardware Preset** as the replacement term for the existing
   hardware-side "controller profile" documentation.
4. Confirm `music-rig` as the CLI name and `pedro-performance-rig` as the first
   Rig identifier.
5. Select the first alternative Device Profile to implement and benchmark.
6. Decide which Arturia pad messages and MIDI channel should be reserved for
   persistent management triggers.
7. Decide the reference machine's acceptable warm-profile memory ceiling and
   which profiles must be pinned for instant live switching.
8. Select the Windows reference machine and the minimum Windows audio, MIDI,
   plugin-host, and service-lifecycle adapter capabilities.
