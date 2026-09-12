# S2 Layout Control Inventory

Status: `2026-09-06` offline inspection only.

This inventory checks whether the engines already present in the protected Carla
project can support the first two S2 layouts. It reads the protected project
XML and does not launch Carla, load a plugin, or change the live rig.

## Existing Engine Entries

| Layout candidate | Carla type | Existing engine | Asset | Exposed controls observed |
| --- | --- | --- | --- | --- |
| `tonewheel-organ` | `SF2` | `AR-CH-6 - Hammond Organ Fast` | Hammond B3 organ SoundFont | Reverb, chorus, polyphony, interpolation |
| `synth-programmer` | `SF2` | `AR-CH-7 - Optik Synth` | Optik Synth SoundFont | Reverb, chorus, polyphony, interpolation |

Both entries are current layers inside `full-live-rack`. Their Carla project
parameters are the same generic SoundFont-player surface:

- Reverb on/off, room size, damping, level, and width;
- Chorus on/off, voice count, level, speed, depth, and type;
- Polyphony; and
- Interpolation.

The project does not expose organ drawbars, percussion, key click, leakage,
rotary speed, oscillator mix, filter cutoff/resonance, envelopes, modulation,
unison, or synth-specific drive controls for these entries.

## Decision

The S2 `tonewheel-organ` and `synth-programmer` documents remain semantic layout
contracts only. Their control targets must not be treated as verified Carla
parameters, and no live activation is allowed from this inventory.

The next engine-selection task must choose one of:

1. a genuinely controllable organ plugin and a genuinely controllable synth
   plugin, with verified parameter metadata and stable assets; or
2. a narrower layout that honestly exposes only the controls available from the
   current SoundFont players.

The first option is the intended product direction. It belongs at the prepared
engine boundary before live layout activation. `full-live-rack` remains the
protected default and is unchanged.
