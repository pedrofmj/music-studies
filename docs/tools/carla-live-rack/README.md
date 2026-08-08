# Carla Live Rack Configurator

This folder contains the reproducible configuration for Pedro's live Carla
rack on `airstar`.

The current deployable project snapshot is versioned under
[`src/audio-software/carla/projects/pedro-live-rack`](../../../src/audio-software/carla/projects/pedro-live-rack/README.md).
SoundFonts and sample libraries remain external.

## Files

- `configure-live-rack.py`: rebuilds `/c/music/carla/pedro.uproject`, validates
  required assets, writes a timestamped backup, and preserves file mode.
- `rack-layout.json`: controller assignments, channel order, and audio routing.
- `inspect-decent-sampler.py`: low-level VST2 state helper used to verify the
  DecentSampler custom-control slots.

## Instrument Order

| Rack | Controller | Instrument | Volume | Reverb |
| --- | --- | --- | --- | --- |
| AR-CH-1 | Arturia | Basic Piano | Fader 1 / CC73 | Knob 1 / CC74 |
| AR-CH-2 | Arturia | Nord White Grand Full 24C | Fader 2 / CC75 | Knob 2 / CC71 |
| AR-CH-3 | Arturia | Alt Strings | Fader 3 / CC79 | Knob 3 / CC76 |
| AR-CH-4 | Arturia | FluidR3 Flute | Fader 4 / CC72 | Knob 4 / CC77 |
| AR-CH-5 | Arturia | SAX Lirakeys CL | Fader 5 / CC80 | Knob 5 / CC93 |
| AR-CH-6 | Arturia | Hammond Organ Fast | Fader 6 / CC81 | Knob 6 / CC18 |
| AR-CH-7 | Arturia | Optik Synth | Fader 7 / CC82 | Knob 7 / CC19 |
| AR-CH-8 | Arturia | PAD EFEITOS | Fader 8 / CC83 | Knob 8 / CC16 |
| AR-CH-9 | Arturia | AtmosferaPAD | Fader 9 / CC85 | Knob 9 / CC17 |
| PD-CH-1 | SMC-PAD / Pocket | Drum Set | CC36 | CC91 |
| SMK-CH-1 | SMK-25 | Oceans Worship Pad | Knob 1 / CC20 | CC91 |
| SMK-CH-2 | SMK-25 | Warm Worship Pad | Knob 2 / CC21 | CC91 |
| SMK-CH-3 | SMK-25 | Hillsong Pad | Knob 3 / CC22 | CC91 |
| SMK-CH-4 | SMK-25 | Worship Shimmer | Knob 4 / CC23 | CC91 |
| SMK-CH-5 | SMK-25 | Cloud Shimmer | Knob 5 / CC24 | CC91 |
| SMK-CH-6 | SMK-25 | Magic Ambient | Knob 6 / CC25 | CC91 |
| SMK-CH-7 | SMK-25 | Majesty Pad | Knob 7 / CC26 | CC91 |
| SMK-CH-8 | SMK-25 | Sanctorium Pad | Knob 8 / CC27 | CC91 |

All instrument rack-level slot volumes are saved at `1.0`. The eight SMK
instruments feed a dedicated `SMK Layer Mixer x8 Stereo`; physical Knobs 1-8
(CC20-27) map directly to its channel gains 1-8. A gain value of zero therefore
hard-mutes the corresponding audio layer instead of depending on an SF2 engine
honoring MIDI CC7. Arturia and pad SF2 volume paths continue to use `mapcc`
conversion. Native SF2 reverb
paths likewise convert each Arturia knob to FluidSynth's standard reverb-send
CC91. The native `Rvrb` engine remains enabled with a fixed return level; its
host parameter is intentionally unmapped so Carla forwards CC91 to the synth.

AR-CH-2 remains saved at 100% slot volume. A controller-independent stereo
trim adds 6 dB after the Nord instrument to compensate for its quieter source
samples without changing the fader's full-range behavior.

The eight SMK layers receive MIDI from the
[SMK-25 Pad Layers](../smk25-pad-layers/README.md) service. Performance data
uses `SMK25-Master`; Stop and Play use the controller AUX `capture_2` endpoint.
The AUX JACK name ends with whitespace, so the PipeWire snapshot watcher owns
that external AUX-to-router link after deployment. Side-A Pads 1-8
control independent continuous hold, while Knobs 1-8 control the corresponding
dedicated mixer channel. The selected SF2 banks all contain looped pad samples.

## Equalizer

`SMC-MIX - 8-Band EQ` is the LSP Parametric Equalizer x8 Stereo. It sits after
the LSP master mixer and before the Arturia rack-only volume/mute gate.

| SMC fader | Input CC | Intermediary output | Band |
| --- | --- | --- | --- |
| 1 | 40 | 102 | 63 Hz low shelf |
| 2 | 41 | 103 | 125 Hz bell |
| 3 | 42 | 104 | 250 Hz bell |
| 4 | 43 | 105 | 500 Hz bell |
| 5 | 44 | 106 | 1 kHz bell |
| 6 | 45 | 107 | 2 kHz bell |
| 7 | 46 | 108 | 4 kHz bell |
| 8 | 47 | 109 | 8 kHz high shelf |

Each fader has a separate `SMC-EQ-N CC Scale` (`mapcc`) plugin. EQ gain is
limited to approximately -12 dB through +12 dB. The display-only FFT streams
are disabled to reduce live CPU usage.

## Audio Graph

```text
AR-CH-2 -> AR-CH-2 Output Trim +6 dB --+
SMK instruments -> SMK Layer Mixer x8 -+
other instruments ---------------------+
                                         |
  -> LSP Mixer x8 Stereo
  -> SMC-MIX - 8-Band EQ
  -> Arturia Main Volume Encoder stereo gate
  -> system playback
```

The central Arturia encoder still controls LSP output gain through CC119, and
its click still mutes only the Carla rack through the stereo gate.

## Apply

Run these commands on `airstar` with Carla closed:

```bash
python3 configure-live-rack.py --check-only /c/music/carla/pedro.uproject
python3 configure-live-rack.py /c/music/carla/pedro.uproject
```

The configurator creates a sibling backup named
`pedro.uproject.before-live-rack-<timestamp>` before writing.

Carla is a Flatpak and `/c` is not shared by default. Launch the project with:

```bash
flatpak run --filesystem=/c studio.kx.carla /c/music/carla/pedro.uproject
```

After changing any live PipeWire connection, save and activate the new graph:

```bash
~/bin/pipewire-patchbay-refresh
```

Dry-run the saved graph without modifying it:

```bash
~/bin/pipewire-patchbay-json --check-and-restore --dry-run
```

## Assets

Most SF2 assets remain under:

```text
~/Flash/PED/MIDI/Pack de Timbres/Library
```

The system FluidR3 bank is copied to the user-visible Flatpak path below so
Carla can load its bank 0, program 73 `Flute` preset without a `/usr` override:

```text
~/.local/share/sounds/sf2/FluidR3_GM.sf2
```

The original 1.3 GB `Good_flute` bank was rejected for this rack because it
kept Carla busy for minutes and used substantially more memory during startup.
