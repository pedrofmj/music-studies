# SMC-Mixer Reference

The SMC-Mixer is an eight-channel MIDI DAW surface. Faders control track
levels, encoders control pan by default, and the track and global buttons
control DAW functions. No audio passes through this unit.

## Architecture

```text
faders / encoders / buttons
  -> DAW mode or User mode
  -> USB-C MIDI or BLE MIDI
  -> Mackie Control or custom MIDI mapping
  -> DAW tracks, transport, and parameters
```

## Controls

| Surface | Default DAW Role |
| --- | --- |
| 8 touch-sensitive faders | Track volume for the current group of eight |
| 8 encoders | Track pan for the current group of eight |
| Track button groups | Mute, solo, record-arm, and select per track |
| Channel Left/Right | Move between channel groups |
| Global buttons | Play, stop, record, rewind, fast-forward, bank navigation, and directional navigation |
| Shift plus Left/Right | Switch DAW mode and User mode |

The manual says an LED flashes when a physical fader does not match the DAW
track volume. Treat that indicator as a pickup warning: move carefully until
the positions agree before making an audible change.

## Mackie Control Recipes

| DAW | Manual Setup |
| --- | --- |
| Ableton Live | Select `MackieControl`; choose `SMC-Mixer` for input and output |
| FL Studio | Enable `SMC-Mixer`; set `Mackie Control Universal`; use the same input/output port |
| Cubase | Add `Mackie Control`; choose `SMC-Mixer` input and output |
| Logic Pro | Install `Mackie Control`; set both ports to `SMC-Mixer` |
| Studio One | Add `Mackie Control`; set receive and send to `SMC-Mixer` |
| Bitwig | Add `Mackie Control`; select `SMC-Mixer` for both ports |
| Reaper | Add `Mackie Control Universal`; select `SMC-Mixer` for both ports |
| Cakewalk | Add `Mackie Control`; select `SMC-Mixer` for both ports |

Use one controller endpoint for one Mackie Control surface. A separate generic
MIDI mapping should use a different, documented mode or endpoint.

## Workflow Recipes

- **First mix:** configure USB, create eight test tracks, confirm fader pickup,
  then test mute, solo, record-arm, select, and transport one at a time.
- **Banked mix:** label the DAW tracks before moving to the next group of eight;
  document the session bank convention in the mapping log.
- **Custom mode:** hold Shift and use the channel buttons to select User mode,
  then map only the controls that have an explicit purpose.

## Linux Notes

The manual lists Windows, macOS, iOS, and Android, not Ubuntu. Validate USB
MIDI first through the shared [Linux workflow](../../mvave-workflows.md#linux-baseline).
Mackie Control is a protocol choice in the DAW; record the actual Linux DAW,
input endpoint, output endpoint, and mode in the mapping log.
