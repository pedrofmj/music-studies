# SMC-PAD Reference

[Structured capabilities](capabilities.json) records source-linked ports, signal directions, connection constraints, and supported roles.

The SMC-PAD is a pad-first MIDI controller with encoder, transport, and MIDI
out capability. It has no internal sound engine.

## Architecture

```text
pads / encoders / transport / note repeat
  -> selected preset, pad bank, and encoder bank
  -> USB-C MIDI, BLE MIDI, or 3.5 mm MIDI out
  -> DAW, software instrument, or hardware MIDI destination
```

## Controls And Modes

| Control | Behavior |
| --- | --- |
| 16 RGB pads | Velocity, aftertouch, and configurable Note, CC, or Program Change output |
| 8 encoders | Assignable endless controls; second bank expands the available assignments |
| Pad Bank / Knob Bank | Select the second pad or encoder bank |
| Play / Stop / Record | DAW transport after Mackie Control input/output are configured |
| Left / Right | Move to the previous or next DAW group of eight tracks |
| Shift plus Pads 1-8 | Select performance, DAW, or user preset configurations |
| Shift plus Pads 9-12 | Select pad velocity curve; Pad 12 is full velocity |
| Shift plus Pads 13-14 | Transpose pad notes up or down |
| Shift plus Pads 15-16 | Shift pad octave range; both reset it |

## Note Repeat Recipe

Enable Note Repeat with the button and a pad. In the Shift plus Note Repeat
layer, pads choose rate, swing, latch, DAW sync, and tap tempo. Encoders 1-4
also control rate, swing, tempo, and latch. The manual specifies a tempo range
of 30 to 300 BPM. Decide whether the clock is local or DAW-synced before a
performance; do not use both as an undocumented fallback.

## Connection And MIDI Out

USB-C is the baseline. BLE requires a host BLE MIDI path. The rear 3.5 mm MIDI
out supports wired hardware MIDI; document the adapter type and target device.
The optional five-pin wireless MIDI adapter is a separate hardware route, not a
replacement for a documented USB or BLE host connection.

## Practical Recipes

- **Phrase and sample triggering:** dedicate a pad bank to non-overlapping
  notes; label color, note, target, and risk level in the mapping log.
- **DAW transport:** use the DAW preset only after configuring the controller
  as Mackie Control for both input and output.
- **Worship transitions:** assign restrained transitions, drones, and cues to a
  dedicated bank. Keep a separate bank for experiments.

## Linux Notes

Start with USB-C and the shared [Linux workflow](../../mvave-workflows.md#linux-baseline).
The vendor manuals do not certify Ubuntu or provide a Linux editor. Verify MIDI
out and BLE independently, then retain all required mapping information locally.
