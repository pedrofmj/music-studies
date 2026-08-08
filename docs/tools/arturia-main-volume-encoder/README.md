# Arturia Main Volume Encoder

`arturia-main-volume-encoder` converts the KeyLab Essential central encoder's
relative channel-1 CC114 messages into absolute CC119 values. Carla maps CC119
to the LSP mixer's output gain, leaving fader 9's CC85 available for another
instrument. The service persists its accumulated value between restarts.

Install or rebuild the user service on airstar with:

```bash
docs/tools/arturia-main-volume-encoder/install-arturia-main-volume-encoder
```

The Patchbay must connect the Arturia MIDI output to
`Arturia Main Volume Encoder:relative-in` and connect
`Arturia Main Volume Encoder:absolute-out` to
`MIDI Scale CC Value:events-in`.
