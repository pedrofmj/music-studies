# FM-1 Reference

[Structured capabilities](capabilities.json) records source-linked ports, signal directions, connection constraints, and supported roles.

The FM-1 is a self-contained sound source and a MIDI device. It should be
documented as both an instrument and a controller.

## Signal Flow

27-key keyboard, USB MIDI, BLE MIDI, and 3.5 mm MIDI input feed the six FM
operators and selected algorithm. Envelopes, LFO, and global synthesis settings
feed the six-slot effect chain, then the built-in speaker or 3.5 mm stereo audio
output. Keyboard, arpeggiator, and sequencer notes also transmit over USB MIDI
and BLE MIDI.

The 3.5 mm rear MIDI connection is MIDI input. The separate 3.5 mm audio
connector is a stereo headphone output. Do not confuse the two or assume that
USB carries audio; the manual documents USB as MIDI.

## Core Architecture

| Area | Documented Behavior |
| --- | --- |
| Operators | Six sine-wave operators; each can act as a carrier or modulator |
| Algorithms | 32 routings that determine carrier, modulator, and feedback relationships |
| Voices | 128 factory presets; changes can be saved to flash |
| Polyphony | Mono or Poly mode, with up to 12 voices in Poly mode |
| Operator editing | Envelope, tuning, sensitivity, output level, scaling, and rate-scale pages |
| Global synthesis | Algorithm, feedback, oscillator sync, and transpose |
| FM SysEx | Receives standard FM voice banks and stores selected data in A/B/C/D bank slots |

## Control Recipes

### Safe FM Editing

1. Begin with a named factory voice and record its number.
2. Change one area at a time: algorithm, then an operator, then envelope or LFO.
3. Compare at matched volume before deciding that a change is better.
4. Save only after recording voice number, algorithm, changed operator, and
   effect state in the backup log.
5. Keep one untouched reference voice for comparison.

### Effects

The effect chain contains Filter, Reverb, Delay, Distortion, Chorus, and Phaser.
SELECT chooses a slot in FX mode; SEL locks a selected effect so it can be moved
in the chain. Use effects after the dry FM tone works. A chain order change can
alter gain and perceived brightness substantially.

### Arpeggiator And Sequencer

The arpeggiator provides seven modes, local or configured timing, gate, swing,
and latch. The sequencer provides 16 patterns with 16 steps. They are mutually
exclusive: disable one before entering the other. Record whether tempo is local
or synchronized, and record the chosen pattern before a live use.

## MIDI And Audio

All three MIDI interfaces are documented as simultaneously active: USB-C MIDI,
BLE MIDI, and 3.5 mm MIDI input. The FM-1 receives note velocity, pitch bend,
CC, and FM SysEx; it sends note messages from the keyboard, arpeggiator, and
sequencer on its configured note channel.

Use the headphone output for audio monitoring or connect it to an audio
interface. Plugging in headphones mutes the internal speaker. Establish this
audio path before treating the FM-1 as part of a live setup.

## Linux Notes

Start with USB-C MIDI and verify the endpoint using the shared
[Linux workflow](../../mvave-workflows.md#linux-baseline). The official
documentation lists Windows, macOS, iOS, and Android, not Ubuntu. Use an audio
interface for FM-1 sound capture unless actual USB audio behavior is observed
and documented. The official FM-1 update package is listed for Windows and macOS;
do not run an unverified firmware path from Linux.