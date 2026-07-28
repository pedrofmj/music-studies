# SMK25 Reference

This reference explains the original SMK25. It is a MIDI controller: keys,
pads, strips, and encoders create MIDI data for a host or configured MIDI
destination; they do not contain instrument sounds.

## Architecture

```text
keys / pads / touch strips / encoders
  -> selected internal preset and bank
  -> USB-B MIDI or BLE MIDI
  -> computer, mobile app, or optional MIDI-out adapter path
  -> software instrument, DAW, or hardware destination
```

The 1/4 inch rear port is sustain by default. The manual states that software
can change this port from pedal to wired MIDI out; that is a role change, so do
not expect sustain while using that port as MIDI out.

## Controls And Limits

| Surface | Practical Use |
| --- | --- |
| 25 velocity-sensitive keys | Melody, chords, bass lines, and software instruments |
| Pitch and modulation strips | Performance expression and assignable CC control |
| 8 pads with Pad Bank | Up to 16 pad assignments for notes, CCs, or program changes |
| 8 encoders with Knob Bank | Up to 16 continuous assignments, including CC and aftertouch |
| Play, Stop, Record | DAW transport after the matching preset and DAW setup are selected |
| Arpeggiator | Patterned note output with type, rate, tempo, swing, gate, latch, and sync |
| Smart Scale and Smart Chord | Constrained scale notes or chord output for quick harmonic ideas |

## Connection Recipes

### USB First

1. Turn the SMK25 on and connect a USB-B data cable.
2. Confirm the host creates a MIDI endpoint.
3. Select a simple piano or synth in the host and verify key velocity.
4. Test one pad and one encoder before loading a DAW preset.
5. Record the endpoint and tested preset in [Mappings](../mappings/README.md).

### BLE MIDI

Hold `BT` until its indicator flashes, pair from the host, and verify a MIDI
endpoint rather than only a Bluetooth pairing. The vendor manual requires
Bluetooth 5.0 and an additional Windows BLE MIDI driver for Windows; it does
not state a Linux editor or Linux BLE support.

### Hardware MIDI Out

The manual describes two paths: reconfigure the rear pedal port to wired MIDI
out in the editor, or use the optional five-pin wireless MIDI adapter. Document
which path is in use. Do not change the pedal port during a performance without
recording how sustain will be handled.

## Performance Recipes

- **Compact worship keys:** use USB to a host or a documented hardware-MIDI
  route, keep one plain piano mapping, reserve a pad bank for low-risk scene or
  sample triggers, and use the modulation strip only after its target CC is
  verified.
- **Arpeggiated texture:** choose rate and gate first, then tempo source. When
  sync is enabled, manual tempo controls are no longer the clock source.
- **Smart chord practice:** set scale and chord type deliberately, then record
  the selected root, major/minor mode, and chord type. Treat it as an arranging
  aid, not an undocumented live dependency.

## Linux Notes

Use the shared [Linux workflow](../../mvave-workflows.md#linux-baseline). USB
is the preferred path. M-VAVE lists editor support for Windows and macOS, not
Ubuntu, so preserve mappings in this repository and validate BLE separately.
