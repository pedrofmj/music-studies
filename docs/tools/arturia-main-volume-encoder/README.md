# Arturia Main Volume Encoder

`arturia-main-volume-encoder` adapts the KeyLab Essential central control for
Carla. It converts the encoder's relative channel-1 CC114 messages into
absolute CC119 values for the LSP mixer's output gain. Each CC115 button press
toggles a stereo audio gate placed between the final EQ and the system output,
leaving fader 9's CC85 available for another instrument. The gate affects only
Carla's routed rack output, not GNOME or other applications.

Install or rebuild the user service on airstar with:

```bash
docs/tools/arturia-main-volume-encoder/install-arturia-main-volume-encoder
```

The service replays its persisted CC119 value whenever `absolute-out` reconnects,
so the current main volume is restored without moving the encoder.

The Patchbay must connect the Arturia MIDI output to
`Arturia Main Volume Encoder:relative-in`, connect
`Arturia Main Volume Encoder:absolute-out` directly to the LSP mixer's MIDI
input, and route the LSP mixer through `SMC-MIX - 8-Band EQ`, then through the
adapter's audio inputs and outputs before the system playback ports.
