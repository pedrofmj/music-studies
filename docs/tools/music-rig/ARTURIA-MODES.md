# Arturia Modes

This guide describes the Arturia KeyLab Essential 61 mk3 modes used with the
Pedro performance rig. The complete Performance Rig reference is at
`src/performance-rigs/pedro-performance-rig/README.md`.

The Arturia pad selector is a Linux user-session service. It does not require
the Arturia Windows editor.

## Start And Stop

The selector service is installed on `airstar`, but it is not enabled at login.

```bash
systemctl --user start music-rig-arturia-profile-session.service
systemctl --user stop music-rig-arturia-profile-session.service
```

Stopping the service restores `full-live-rack` and removes temporary candidate
connections.

## Pad Modes

Pad management matches MIDI channel 10 only. Normal keyboard notes use channel 1
and are not interpreted as mode changes.

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

## Full Live Rack

This is the protected default and recovery mode.

| Fader | Instrument volume | Knob | Instrument reverb |
| --- | --- | --- | --- |
| 1 | Basic Piano | 1 | Basic Piano |
| 2 | Nord White Grand | 2 | Nord White Grand |
| 3 | Alt Strings | 3 | Alt Strings |
| 4 | Good Flute | 4 | Good Flute |
| 5 | SAX Lirakeys | 5 | SAX Lirakeys |
| 6 | Hammond Organ Fast | 6 | Hammond Organ Fast |
| 7 | Optik Synth | 7 | Optik Synth |
| 8 | PAD EFEITOS | 8 | PAD EFEITOS |
| 9 | AtmosferaPAD | 9 | AtmosferaPAD |

## Synth Programmer

Faders use pickup behavior: the value changes after the physical control crosses
the stored value.

| Control | Parameter |
| --- | --- |
| Fader 1 | Oscillator 1 mix, `DCO1_BALANCE` |
| Fader 2 | Oscillator 2 level/balance, `DCO2_BALANCE` |
| Fader 3 | Filter cutoff, `DCF1_CUTOFF` |
| Fader 4 | Filter resonance, `DCF1_RESO` |
| Fader 5 | Amp attack, `DCA1_ATTACK` |
| Fader 6 | Amp release, `DCA1_RELEASE` |
| Fader 7 | Modulation depth, `LFO1_BALANCE` |
| Fader 8 | Modulation rate, `LFO1_RATE` |
| Fader 9 | Unison/detune |
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

The organ uses setBfree. Drawbars are inverted like a physical organ: down is
loudest and up is off.

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

## Materialized Genre Modes

Pads 4 through 16 load separate headless Carla projects. Each project has nine
real SoundFont or DecentSampler patches from the larger library. These are not
the nine live-rack instruments. The source paths are recorded in
`benchmarks/arturia-genre-patches.json`.

In every genre mode, Faders 1-9 control the volume of the nine patches below and
Knobs 1-9 control their corresponding reverb. The central encoder remains Master
volume and its click remains Master mute.

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

SMK-25, SMC-Mixer, SMC-PAD, and SMC-PAD Pocket are not part of these Arturia
mode changes. Their existing MIDI, audio, pad, transport, and mixer behavior
remains active.

## Headless Carla

The protected full rack can also run as an opt-in headless Carla user service.
Close normal GUI Carla first.

```bash
docs/tools/music-rig/packaging/linux/install-pedro-carla-headless
systemctl --user start music-rig-pedro-carla-headless.service
~/.local/bin/carla-pedro-osc-gui
```

The OSC GUI attaches to the running backend; it does not launch a second Carla
engine. Stop the backend before reopening normal GUI Carla:

```bash
systemctl --user stop music-rig-pedro-carla-headless.service
```

## Safety

- `full-live-rack` remains the default and rollback mode.
- Candidate engines are user-owned and are not installed system-wide by the
  session installer.
- The service is manually started and can be stopped immediately.
- The protected Carla project and current live services are not replaced by a
  pad mode.
