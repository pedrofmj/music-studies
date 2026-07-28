# SMC-PAD Pocket Reference

The SMC-PAD Pocket is a compact 16-pad MIDI controller. It sends MIDI over
USB-C or BLE and does not make sound itself.

## Architecture

```text
16 pads and assigned control pads
  -> startup preset and one of seven pad banks
  -> USB-C MIDI or BLE MIDI
  -> DAW, sampler, virtual instrument, or documented MIDI destination
```

## Pad And Bank Model

The official manual specifies seven banks of 16 pads, yielding 112 MIDI-note
positions. A pad can operate in note mode or control mode. Control assignments
include Note, CC toggle, momentary CC, Program Change, and custom SysEx. The
editor also defines channel, note number, velocity limits, color, pad curve,
and aftertouch behavior.

## Startup Presets

Hold one of pads 1-4 while powering on to select a startup preset:

| Preset | Intended Factory Role |
| --- | --- |
| 1 | Ableton Live Drum Rack mapping |
| 2 | FL Studio FPC drum mapping |
| 3 | GarageBand drum mapping |
| 4 | Pads 9-16 used as control buttons |

Treat these as starting points. Verify their actual output on the intended host
before assigning them to a live role.

## Note Repeat And Control Recipe

The dedicated control assignment can expose note repeat, rate, swing, bank
selection, and latch. In the editor, note repeat rate is relative to tempo,
swing is adjustable, sync can follow an external MIDI clock, and latch can keep
repetition active after release. Use one visible pad color for each control
class and record the entire bank in [Mappings](../mappings/README.md).

## Connection Recipes

### USB-C

Connect by USB-C, confirm that the host sees MIDI, and test one pad in the
lowest and highest configured bank. The manual describes it as class-compliant
for USB MIDI; vendor editor support is documented for Windows and macOS.

### BLE MIDI

Pair the controller and then confirm a MIDI endpoint. The vendor manual states
that Windows wireless use requires its BT MIDI Connector, while macOS uses
Audio MIDI Setup. On Linux, use the shared verification process rather than
assuming the vendor desktop path is available.

### MIDI Out Caveat

The manual describes optional five-pin wireless MIDI adapter use. It also says
that connection to that adapter prevents concurrent connection to another host.
Treat the adapter as a mutually exclusive destination and document it clearly.

## Linux Notes

Use the shared [Linux workflow](../../mvave-workflows.md#linux-baseline). USB
is the reliable starting point; BLE MIDI needs an observed host endpoint. Keep
custom mappings in the repository because M-VAVE does not list a native Linux
editor.
