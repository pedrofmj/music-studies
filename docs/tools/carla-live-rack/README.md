# Carla Live Rack Configurator

This folder contains the reproducible configuration for Pedro's live Carla
rack on `airstar`.

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

The nine Arturia rack-level slot volumes and both additional controller slots
are saved at `1.0`. Native SF2 volume paths use a `mapcc` stage that converts
the physical control to standard instrument volume CC7.

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
all instruments
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

