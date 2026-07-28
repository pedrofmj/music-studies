# TEYUN A8 Learning

Use this sequence only after checking that the physical unit matches the
provisional reference. Record every observed connector, button label, and
channel number in [Routing](../routing/README.md).

## 1. Identify The Revision

1. Photograph the front, rear, and bottom labels.
2. Record every input, output, USB port, power port, and phantom-power switch.
3. Confirm the printed model, channel count, and power requirement.
4. Compare these findings with [Capabilities](../reference/capabilities.json).

## 2. One Safe Analog Source

1. Turn channel level, gain, MAIN, and headphones down.
2. Connect XPS-30 OUTPUT L/MONO to one confirmed line input.
3. Keep phantom power off unless a confirmed condenser microphone needs it.
4. Play the loudest expected notes and raise gain only until the channel PEAK
   indicator is not continuously lit.
5. Raise channel level, then MAIN, then the downstream powered-speaker or
   amplifier level.

## 3. Stereo Keyboard And FM Source

1. Put XPS-30 L/MONO and R on two matched line inputs.
2. Connect FM-1 headphone output with a 3.5 mm TRS-to-two-mono breakout to two
   other confirmed line inputs.
3. Begin the FM-1 master volume low; its headphone output can be hotter than a
   normal line source.
4. Pan or route channels only after confirming the actual A8 channel controls.
5. Set source levels first, then channel gains, then the main output level.

## 4. USB Audio On Ubuntu

1. Connect the A8 USB data port directly to the computer.
2. Run `arecord -l`, `aplay -l`, and `wpctl status`.
3. Record the actual capture/playback device names and available channel counts.
4. Make a short capture with one XPS-30 input and verify whether it contains a
   stereo mix, a selected bus, or another signal path.
5. Test computer playback through the A8 at low level before using it live.

Do not assume multitrack recording, DSP inclusion in USB capture, loopback
behavior, or direct monitoring until the unit has been observed.
