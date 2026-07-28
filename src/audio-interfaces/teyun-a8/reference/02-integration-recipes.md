# Integration Recipes

These recipes use only conservative, line-level audio practices. Confirm actual
A8 labels before assigning channel numbers.

## XPS-30 Mono Worship Setup

```text
XPS-30 OUTPUT L/MONO -> A8 confirmed line input -> A8 MAIN OUT -> PA chain
Roland DP-10         -> XPS-30 PEDAL HOLD
```

Use this when the PA is mono or the mixer has only one practical input channel.
Keep the XPS-30 and A8 effects modest at first; layer and reverb decisions must
be made in the room, not only on headphones.

## XPS-30 And FM-1 Stereo Submix

```text
XPS-30 OUTPUT L + R  -> two matched A8 line inputs
FM-1 headphone out   -> 3.5 mm TRS-to-two-mono breakout -> two A8 line inputs
A8 MAIN OUT L + R    -> two PA/mixer line inputs or powered speakers
```

The FM-1 headphone output is a stereo source, while ordinary mixer line inputs
are mono. Use a breakout cable with left and right mono plugs. Begin with FM-1
master level low and raise it only enough to achieve a clean channel gain.

## Computer Recording Of A Live Stereo Mix

```text
XPS-30 / FM-1 -> A8 analog inputs -> provisional A8 USB audio -> Ubuntu computer
A8 MAIN OUT   -> PA or monitors
```

Record a test first. The third-party documentation does not establish whether
USB capture includes DSP, Bluetooth, computer playback, or a final stereo mix.
Never assume this setup captures separate XPS-30 and FM-1 tracks.

## Computer Playback Into The Live Rig

```text
Ubuntu computer -> provisional A8 USB playback -> A8 MAIN OUT -> PA/monitors
```

Start the computer and A8 at low levels. Confirm the selected PipeWire device,
test a known audio file, and ensure no loopback or feedback route feeds the
computer output back into a live-recording input.
