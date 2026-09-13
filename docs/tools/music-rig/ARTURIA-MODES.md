# Arturia Modes

This guide describes the Arturia KeyLab Essential 61 mk3 modes used with the
Pedro performance rig.

The Arturia pad selector is a Linux user-session service. It does not require
the Arturia Windows editor.

## Start And Stop

The service is installed on `airstar` but is intentionally not enabled at login.

Start the selector before a performance:

```bash
systemctl --user start music-rig-arturia-profile-session.service
```

Stop it after the performance:

```bash
systemctl --user stop music-rig-arturia-profile-session.service
```

Stopping the service restores `full-live-rack` and removes temporary candidate
connections.

## Headless Carla

The protected full rack can also be launched as an opt-in headless Carla user
service. This is separate from the Arturia pad selector and must not run beside
the normal GUI Carla rack.

Install the optional launcher:

```bash
docs/tools/music-rig/packaging/linux/install-pedro-carla-headless
```

With the normal Carla GUI rack closed, start the headless backend:

```bash
systemctl --user start music-rig-pedro-carla-headless.service
```

The optional Carla control GUI can then attach to the running OSC backend:

```bash
~/.local/bin/carla-pedro-osc-gui
```

Stop the backend before returning to the normal GUI rack:

```bash
systemctl --user stop music-rig-pedro-carla-headless.service
```

The headless service is not enabled automatically. Opening the project with a
second normal Carla GUI would create a second engine instead of attaching to the
headless backend.

## Pad Modes

Use Bank A or Bank B on the Arturia pads. Pad switching matches MIDI channel 10
only. Keyboard notes use channel 1 and are not interpreted as mode changes.

| Pad | Mode |
| --- | --- |
| Bank A Pad 1 | Tonewheel Organ |
| Bank A Pad 2 | Synth Programmer |
| Bank A Pad 3 | Full Live Rack |
| Bank A Pad 4 | Worship Piano |
| Bank A Pad 5 | Gospel Keys |
| Bank A Pad 6 | Ambient Worship |
| Bank A Pad 7 | Jazz Keys |
| Bank A Pad 8 | Soul and R&B |
| Bank B Pad 1 | Cinematic Strings |
| Bank B Pad 2 | Orchestral |
| Bank B Pad 3 | Brass and Winds |
| Bank B Pad 4 | Synthwave |
| Bank B Pad 5 | Retro Keys |
| Bank B Pad 6 | Intimate Pads |
| Bank B Pad 7 | Praise Leads |
| Bank B Pad 8 | Acoustic Worship |

The selector consumes only the matching channel-10 pad event. Normal keyboard
playing and controller messages continue through the active Arturia engine.

## Full Live Rack

This is the current default and recovery mode. The Arturia faders control layer
volume, and the knobs control layer reverb.

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | Good Flute volume |
| Fader 5 | SAX Lirakeys volume |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | Optik Synth volume |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | Good Flute reverb |
| Knob 5 | SAX Lirakeys reverb |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | Optik Synth reverb |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |
| Central encoder | Master volume |
| Central encoder click | Master mute |

## Synth Programmer

The synth uses synthv1 with a native controller map. Faders use pickup behavior:
the parameter changes after the physical control crosses its stored value.

| Control | Synth parameter |
| --- | --- |
| Fader 1 | Oscillator 1 mix, `DCO1_BALANCE` |
| Fader 2 | Oscillator 2 level/balance, `DCO2_BALANCE` |
| Fader 3 | Filter cutoff, `DCF1_CUTOFF` |
| Fader 4 | Filter resonance, `DCF1_RESO` |
| Fader 5 | Amp envelope attack, `DCA1_ATTACK` |
| Fader 6 | Amp envelope release, `DCA1_RELEASE` |
| Fader 7 | Modulation depth, `LFO1_BALANCE` |
| Fader 8 | Modulation rate, `LFO1_RATE` |
| Fader 9 | Unison/detune control |
| Knob 1 | Drive/compression |
| Knob 2 | Effects send/mix |
| Knob 3 | Delay time |
| Knob 4 | Delay feedback |
| Knob 5 | Reverb size |
| Knob 6 | Reverb mix |
| Knob 7 | Pitch modulation |
| Knob 8 | Synth volume, `OUT1_VOLUME` |
| Knob 9 | Synth reverb width |

## Tonewheel Organ

The organ uses setBfree. The nine faders are inverted automatically so they
behave like real drawbars: the bottom position is loudest and the top position
is off.

| Control | Organ function |
| --- | --- |
| Fader 1 | 16' drawbar |
| Fader 2 | 5 1/3' drawbar |
| Fader 3 | 8' drawbar |
| Fader 4 | 4' drawbar |
| Fader 5 | 2 2/3' drawbar |
| Fader 6 | 2' drawbar |
| Fader 7 | 1 3/5' drawbar |
| Fader 8 | 1 1/3' drawbar |
| Fader 9 | 1' drawbar |
| Knob 1 | Percussion enable/mode |
| Knob 2 | Percussion decay |
| Knob 3 | Overdrive input |
| Knob 4 | Vibrato/chorus selection |
| Knob 5 | Rotary stop/slow/fast |
| Knob 6 | Overdrive output |
| Knob 7 | Vibrato routing |
| Knob 8 | Swell volume |
| Knob 9 | Reverb mix |

## Genre Modes

Pads 4 through 16 load separate materialized Carla projects. Each project has
nine concrete SoundFont or DecentSampler patches selected from the larger
library, rather than reusing the nine live-rack patches. The exact source paths
are recorded in `benchmarks/arturia-genre-patches.json`.

They do not replace the SMK-25, SMC-Mixer, SMC-PAD, or SMC-PAD Pocket profiles.
Those controllers remain active with their existing behavior.

The live session runner swaps only Arturia layer audio links; it does not mute
the shared master output. The complete musician-facing patch table is also
included in the Performance Rig README at `src/performance-rigs/pedro-performance-rig/README.md`.

The central encoder remains Master volume and its click remains Master mute in
every mode. The older control-slot tables below describe the original live-rack
layer controls; for the actual materialized genre patch names, use the table in
the Performance Rig README.

### Worship Piano

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | inactive |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | inactive |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Gospel Keys

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | SAX Lirakeys volume |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | Optik Synth volume |
| Fader 8 | inactive |
| Fader 9 | inactive |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | SAX Lirakeys reverb |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | Optik Synth reverb |
| Knob 8 | inactive |
| Knob 9 | inactive |

### Ambient Worship

| Control | Function |
| --- | --- |
| Fader 1 | inactive |
| Fader 2 | inactive |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | inactive |
| Fader 7 | Optik Synth volume |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | inactive |
| Knob 2 | inactive |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | inactive |
| Knob 7 | Optik Synth reverb |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Jazz Keys

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | Good Flute volume |
| Fader 5 | SAX Lirakeys volume |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | inactive |
| Fader 8 | inactive |
| Fader 9 | inactive |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | Good Flute reverb |
| Knob 5 | SAX Lirakeys reverb |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | inactive |
| Knob 8 | inactive |
| Knob 9 | inactive |

### Soul And R&B

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | SAX Lirakeys volume |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | Optik Synth volume |
| Fader 8 | inactive |
| Fader 9 | inactive |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | SAX Lirakeys reverb |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | Optik Synth reverb |
| Knob 8 | inactive |
| Knob 9 | inactive |

### Cinematic Strings

| Control | Function |
| --- | --- |
| Fader 1 | inactive |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | inactive |
| Fader 7 | Optik Synth volume |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | inactive |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | inactive |
| Knob 7 | Optik Synth reverb |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Orchestral

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | Good Flute volume |
| Fader 5 | SAX Lirakeys volume |
| Fader 6 | inactive |
| Fader 7 | inactive |
| Fader 8 | inactive |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | Good Flute reverb |
| Knob 5 | SAX Lirakeys reverb |
| Knob 6 | inactive |
| Knob 7 | inactive |
| Knob 8 | inactive |
| Knob 9 | AtmosferaPAD reverb |

### Brass And Winds

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | inactive |
| Fader 3 | Alt Strings volume |
| Fader 4 | Good Flute volume |
| Fader 5 | SAX Lirakeys volume |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | Optik Synth volume |
| Fader 8 | inactive |
| Fader 9 | inactive |
| Knob 1 | Basic Piano reverb |
| Knob 2 | inactive |
| Knob 3 | Alt Strings reverb |
| Knob 4 | Good Flute reverb |
| Knob 5 | SAX Lirakeys reverb |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | Optik Synth reverb |
| Knob 8 | inactive |
| Knob 9 | inactive |

### Synthwave

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | inactive |
| Fader 7 | Optik Synth volume |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | inactive |
| Knob 7 | Optik Synth reverb |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Retro Keys

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | inactive |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | Optik Synth volume |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | inactive |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | Optik Synth reverb |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Intimate Pads

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | inactive |
| Fader 7 | inactive |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | inactive |
| Knob 7 | inactive |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Praise Leads

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | inactive |
| Fader 3 | inactive |
| Fader 4 | inactive |
| Fader 5 | inactive |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | Optik Synth volume |
| Fader 8 | PAD EFEITOS volume |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | inactive |
| Knob 3 | inactive |
| Knob 4 | inactive |
| Knob 5 | inactive |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | Optik Synth reverb |
| Knob 8 | PAD EFEITOS reverb |
| Knob 9 | AtmosferaPAD reverb |

### Acoustic Worship

| Control | Function |
| --- | --- |
| Fader 1 | Basic Piano volume |
| Fader 2 | Nord White Grand volume |
| Fader 3 | Alt Strings volume |
| Fader 4 | Good Flute volume |
| Fader 5 | inactive |
| Fader 6 | Hammond Organ Fast volume |
| Fader 7 | inactive |
| Fader 8 | inactive |
| Fader 9 | AtmosferaPAD volume |
| Knob 1 | Basic Piano reverb |
| Knob 2 | Nord White Grand reverb |
| Knob 3 | Alt Strings reverb |
| Knob 4 | Good Flute reverb |
| Knob 5 | inactive |
| Knob 6 | Hammond Organ Fast reverb |
| Knob 7 | inactive |
| Knob 8 | inactive |
| Knob 9 | AtmosferaPAD reverb |

## Safety

- `full-live-rack` remains the default and rollback mode.
- Candidate engines are staged in a user-owned directory and are not installed
  system-wide by the session installer.
- The service is manually started and can be stopped immediately.
- The protected Carla project and current live services are not replaced by a
  pad mode.
