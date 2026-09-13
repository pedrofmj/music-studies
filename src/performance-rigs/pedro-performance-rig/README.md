# Pedro Performance Rig

This directory contains the portable authored definition of the first
Performance Rig. Schema version `music-studies/performance-rig/v1` uses JSON
Schema Draft 2020-12.

Current status: schema, stable-slot, Hardware Preset, Device Profile, Rig Profile,
Linux Platform Binding extraction, deterministic compilation envelope, pad
triggers, mapped synthv1/setBfree candidates, and materialized genre projects.
The authored files remain the source of truth for candidate selection; the
protected `setup.json`, Carla project, services, and graph remain the production
authority and default recovery state.

`rig.json` records the five stable controller slots from the protected rack.
Each slot orders selectors from model, semantic alias, and endpoint purpose to
an optional local discriminator and optional USB ID. The four M-VAVE devices
share USB ID `4353:4b4d`, so that ID is supporting evidence and cannot
distinguish them by itself. Platform bindings resolve these portable selectors
to operating-system device identities.

All six Hardware Preset IDs resolve to verified files in `hardware-presets`.
The SMC-PAD and SMC-PAD Pocket assignments are checked against the
[2026-08-11 live capture](../../../docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json),
including exact per-pad channel/note assignments and the Pocket's eight
hardware-internal control pads.

The current Device Profiles resolve under `device-profiles/<slot>/<id>`:

- Arturia `multi-instrument-rack`;
- SMK-25 `ambient-pad-layers`;
- SMC-Mixer `eight-band-eq`;
- SMC-PAD `drum-set`; and
- SMC-PAD Pocket `drum-set`.

S2 adds Arturia candidate profiles:

- `tonewheel-organ`, with nine drawbar mappings and organ controls;
- `tonewheel-organ-setbfree`, with the same Arturia layout and a setBfree MIDI backend;
- `synth-programmer`, with oscillator, filter, envelope, modulation, and effects mappings;
- `synth-programmer-synthv1`, with a native synthv1 controller map; and
- thirteen genre profiles using materialized SoundFont/DecentSampler patch sets.

The setBfree and synthv1 candidates use their own headless engines and the
verified Arturia hardware control surface. Genre profiles use separate candidate
Carla projects generated from `docs/tools/music-rig/benchmarks/arturia-genre-patches.json`.
All candidates remain explicit, reversible sessions; the protected live Carla
project is not replaced.

They link semantic mappings to controls from the selected Hardware Preset and
declare capabilities, ownership, dependencies, readiness, state, takeover, and
switch safety without platform paths or backend identifiers. The two pad
profiles intentionally share `drum-set.notes`; other current ownership remains
exclusive or read-only.

[`rig-profiles/full-live-rack.json`](rig-profiles/full-live-rack.json) composes
all five roles as the default and fallback Rig Profile. It declares the
aggregate endpoint and musical capabilities, pins the eighteen current sound
engines, identifies the shared drum engine and effects, and retains the safe
takeover and rollback policies. It is authored data only; the protected setup
remains the active default. The resolved
[`switch-triggers.json`](switch-triggers.json) management catalogue maps the
16 Arturia pads to the organ, synth, live, and genre profiles.

[`rig-profiles/tonewheel-organ.json`](rig-profiles/tonewheel-organ.json),
[`rig-profiles/tonewheel-organ-setbfree.json`](rig-profiles/tonewheel-organ-setbfree.json),
and the synth/genre profiles compose Arturia alternatives with the unchanged
SMK-25, SMC-Mixer, SMC-PAD, and Pocket roles. `full-live-rack` remains the
protected default and fallback.

The seven schemas are:

- `common.schema.json`: shared identifiers, selectors, capabilities, MIDI,
  ownership, readiness, takeover, state, and switch-safety types;
- `rig.schema.json`: the complete Rig catalogue and stable device slots;
- `rig-profile.schema.json`: one global composition;
- `device-profile.schema.json`: one role for one logical device slot;
- `hardware-preset.schema.json`: controller-local raw message assignments;
- `platform-binding.schema.json`: backend device identities, semantic target
  locators, machine paths, lifecycle resources, and evidence status; and
- `switch-triggers.schema.json`: persistent management events translated to
  the same future switch operations as the CLI.

Top-level authored definitions contain semantic capabilities. Operating-system
paths, PipeWire/JACK port names, Carla parameter indices, Windows device IDs,
and service identifiers belong only in Platform Bindings. The authored
[`airstar-current`](platform-bindings/linux/airstar-current.json) Linux binding
resolves the complete `full-live-rack` contract against the protected setup.
It is authoring-only and cannot mutate or activate the runtime. The Windows
document under the validator fixtures proves the portable contract shape; it
is explicitly not physical Windows support or certification.

Root validation checks the Rig, all Hardware Presets, Device Profiles, Rig
Profiles, Platform Bindings, and Switch Triggers. It resolves slot, model,
endpoint, preset, control, profile, capability, resource, binding, trigger
source, and trigger-operation references; checks required-slot coverage,
aggregate capabilities and readiness, pinned and shared resources,
initial-state ownership, explicit composition ownership conflicts, management
MIDI assignment, readiness ceilings, and consumed-event mapping conflicts; and
locks the current Linux binding to protected Airstar aliases, paths, checksums,
and services. It reads authored files and protected evidence only; it does not
connect to the live rig.

Run the offline schema suite from the repository root after installing the
authoring-only dependency:

```bash
python3 -m pip install -r docs/tools/music-rig/requirements-schema.txt
python3 docs/tools/music-rig/validate-performance-rig.py --self-test
python3 docs/tools/music-rig/validate-performance-rig.py \
  --validate-root src/performance-rigs/pedro-performance-rig \
  --authority-setup docs/tools/airstar-live-setup/setup.json \
  --authority-pad-capture \
    docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json

python3 docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --check-only
```

The compiler emits only to an explicit output path and refuses to overwrite
authored source. Its Milestone 2 lookup, ownership, graph-delta, and fingerprint
contracts are documented in
[`COMPILER.md`](../../../docs/tools/music-rig/COMPILER.md). It does not
materialize or activate Carla, Patchbay, MIDI, audio, service, or runtime state.

## Musician Guide

The Arturia KeyLab Essential 61 mk3 can select the following modes from its
16 pads. Bank A and Bank B each contain eight pads. Pad management uses MIDI
channel 10; normal keyboard notes use channel 1 and are not mode triggers.

| Pad | Mode |
| --- | --- |
| Bank A 1 | Tonewheel Organ (`tonewheel-organ-setbfree`) |
| Bank A 2 | Synth Programmer (`synth-programmer-synthv1`) |
| Bank A 3 | Full Live Rack (`full-live-rack`) |
| Bank A 4 | Worship Piano (`worship-piano`) |
| Bank A 5 | Gospel Keys (`gospel-keys`) |
| Bank A 6 | Ambient Worship (`ambient-worship`) |
| Bank A 7 | Jazz Keys (`jazz-keys`) |
| Bank A 8 | Soul and R&B (`soul-rnb`) |
| Bank B 1 | Cinematic Strings (`cinematic-strings`) |
| Bank B 2 | Orchestral (`orchestral`) |
| Bank B 3 | Brass and Winds (`brass-winds`) |
| Bank B 4 | Synthwave (`synthwave`) |
| Bank B 5 | Retro Keys (`retro-keys`) |
| Bank B 6 | Intimate Pads (`intimate-pads`) |
| Bank B 7 | Praise Leads (`praise-leads`) |
| Bank B 8 | Acoustic Worship (`acoustic-worship`) |

Start the explicit Linux user session on `airstar` before playing:

```bash
systemctl --user start music-rig-arturia-profile-session.service
```

Stop it after playing. This restores `full-live-rack` and removes candidate
processes and links:

```bash
systemctl --user stop music-rig-arturia-profile-session.service
```

The central Arturia encoder is Master volume in every mode. Pressing it toggles
Master mute. The faders and knobs below use the same physical positions in every
genre project: each fader controls the volume of its listed patch, and the knob
below it controls that patch's reverb.

### Full Live Rack

This is the protected current default and recovery mode.

| Fader | Instrument | Knob | Function |
| --- | --- | --- | --- |
| 1 | Basic Piano | 1 | Basic Piano reverb |
| 2 | Nord White Grand | 2 | Nord White Grand reverb |
| 3 | Alt Strings | 3 | Alt Strings reverb |
| 4 | Good Flute | 4 | Good Flute reverb |
| 5 | SAX Lirakeys | 5 | SAX Lirakeys reverb |
| 6 | Hammond Organ Fast | 6 | Hammond Organ Fast reverb |
| 7 | Optik Synth | 7 | Optik Synth reverb |
| 8 | PAD EFEITOS | 8 | PAD EFEITOS reverb |
| 9 | AtmosferaPAD | 9 | AtmosferaPAD reverb |

### Synth Programmer

| Control | Function |
| --- | --- |
| Fader 1 | Oscillator 1 mix (`DCO1_BALANCE`) |
| Fader 2 | Oscillator 2 level/balance (`DCO2_BALANCE`) |
| Fader 3 | Filter cutoff (`DCF1_CUTOFF`) |
| Fader 4 | Filter resonance (`DCF1_RESO`) |
| Fader 5 | Amp attack (`DCA1_ATTACK`) |
| Fader 6 | Amp release (`DCA1_RELEASE`) |
| Fader 7 | Modulation depth (`LFO1_BALANCE`) |
| Fader 8 | Modulation rate (`LFO1_RATE`) |
| Fader 9 | Unison/detune |
| Knob 1 | Drive/compression |
| Knob 2 | Effects send/mix |
| Knob 3 | Delay time |
| Knob 4 | Delay feedback |
| Knob 5 | Reverb size |
| Knob 6 | Reverb mix |
| Knob 7 | Pitch modulation |
| Knob 8 | Synth volume (`OUT1_VOLUME`) |
| Knob 9 | Synth reverb width |

### Tonewheel Organ

| Fader | Drawbar | Knob | Function |
| --- | --- | --- | --- |
| 1 | 16' | 1 | Percussion enable/mode |
| 2 | 5 1/3' | 2 | Percussion decay |
| 3 | 8' | 3 | Overdrive input |
| 4 | 4' | 4 | Vibrato/chorus selection |
| 5 | 2 2/3' | 5 | Rotary stop/slow/fast |
| 6 | 2' | 6 | Overdrive output |
| 7 | 1 3/5' | 7 | Vibrato routing |
| 8 | 1 1/3' | 8 | Swell volume |
| 9 | 1' | 9 | Reverb mix |

Drawbars are inverted like a physical organ: down is loudest and up is off.

### Genre Patch Sets

Pads 4 through 16 use materialized Carla projects built from the installed
SoundFont and DecentSampler libraries. The exact patch source paths are recorded
in [`arturia-genre-patches.json`](../../../docs/tools/music-rig/benchmarks/arturia-genre-patches.json).

| Mode | Faders 1 through 9, in order |
| --- | --- |
| Worship Piano | Wurlitzer, CFX Grand, Slinky Violin Duet, JF Gospel Organ, Cinematic Strings, JF Worship Piano, Sky Pad Evolving, Endless Church Chime, Worship Guitar |
| Gospel Keys | Wurlitzer, Motif ES6 Piano, Slinky Violin Duet, JF Gospel Organ, Alto Sax, Dark Violins, Choir Korg Aahhs, Laboriel Slap Bass, Shiny Effects Pad |
| Ambient Worship | Sky Pad Evolving, CFX Grand, Subfrost Harmonic Bow, Cinematic Strings, Shiny Effects Pad, CS-20M Big Waves, JF Worship Piano, Endless Church Chime, Choir Korg Aahhs |
| Jazz Keys | Wurlitzer, Rhodes VS Extreme, Slinky Violin Duet, Motif ES6 Piano, Alto Sax, Dark Violins, Worship Guitar, Short Scale Bass, Endless Church Chime |
| Soul and R&B | Wurlitzer, Rhodes VS Extreme, Slinky Violin Duet, JF Worship Piano, Alto Sax, Laboriel Slap Bass, Worship Guitar, Dark Violins, Shiny Effects Pad |
| Cinematic Strings | Sky Pad Evolving, Dark Violins, Subfrost Harmonic Bow, CFX Grand, Cinematic Strings, Choir Korg Aahhs, Endless Church Chime, D-50 Stack, Motif ES6 Piano |
| Orchestral | Box Harp Picked, Cinematic Strings, Slinky Violin Duet, Alto Sax, Mariachi Trumpet, Choir Korg Aahhs, Endless Church Chime, CFX Grand, Subfrost Harmonic Bow |
| Brass and Winds | Wurlitzer, Alto Sax, CS-20M Big Waves, Motif ES6 Piano, Worship Guitar, Laboriel Slap Bass, Dark Violins, Endless Church Chime, Mariachi Trumpet |
| Synthwave | CS-20M Big Waves, D-50 Stack, Sky Pad Evolving, Rhodes VS Extreme, Wurlitzer, Shiny Effects Pad, Motif ES6 Piano, Laboriel Slap Bass, Endless Church Chime |
| Retro Keys | Wurlitzer, Rhodes VS Extreme, CS-20M Big Waves, Motif ES6 Piano, Hammond B3 Slow, JF Super Saw, Laboriel Slap Bass, Shiny Effects Pad, Endless Church Chime |
| Intimate Pads | Wurlitzer, CFX Grand, Slinky Violin Duet, Dark Violins, Sky Pad Evolving, Subfrost Harmonic Bow, Box Harp Picked, Endless Church Chime, Choir Korg Aahhs |
| Praise Leads | CS-20M Big Waves, JF Gospel Organ, Sky Pad Evolving, D-50 Stack, JF Worship Piano, Cinematic Strings, Shiny Effects Pad, Choir Korg Aahhs, Endless Church Chime |
| Acoustic Worship | Wurlitzer, CFX Grand, Slinky Violin Duet, Dark Violins, Worship Guitar, JF Gospel Organ, Alto Sax, Box Harp Picked, Sky Pad Evolving |

The SMK-25, SMC-Mixer, SMC-PAD, and SMC-PAD Pocket are not part of these
Arturia mode changes. Their existing MIDI, audio, pad, transport, and mixer
behavior remains active.

### Headless Carla

The protected live rack and genre projects can run headlessly. To install the
optional headless full-rack service:

```bash
docs/tools/music-rig/packaging/linux/install-pedro-carla-headless
```

Close normal GUI Carla first, then start the headless backend:

```bash
systemctl --user start music-rig-pedro-carla-headless.service
```

To attach the optional Carla control GUI through OSC:

```bash
~/.local/bin/carla-pedro-osc-gui
```

Do not open the same project in a second normal Carla instance. Stop the
headless service before returning to GUI Carla:

```bash
systemctl --user stop music-rig-pedro-carla-headless.service
```
