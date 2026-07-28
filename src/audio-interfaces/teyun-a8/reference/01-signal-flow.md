# Signal Flow And Safety

The A8 is an **audio** mixer. It accepts analog and claimed digital-playback
sources, shapes their levels and tone, then sends a mixed audio signal to MAIN,
headphones, and potentially USB audio. It does not carry MIDI.

```text
XPS-30 / FM-1 / microphones / playback
  -> A8 input channel and gain
  -> channel EQ, level, and claimed DSP send
  -> MAIN mix / headphones / provisional USB capture
  -> powered speaker, PA, recorder, or computer playback path
```

## Gain Staging

1. Start with source volume, channel gain, channel level, MAIN, and downstream
   speaker level low.
2. Set the source to a realistic playing level.
3. Raise channel gain until PEAK is not continuously active on loud material.
4. Balance channels using their level controls.
5. Raise MAIN only after the channel mix is balanced.
6. Set the powered-speaker or PA input last.

Gain is not a volume substitute. Excessive gain clips an input before channel
or main level controls can repair it.

## Phantom Power

The available third-party sources claim 48 V phantom power but do not provide a
reliable A8-revision-specific scope. Leave it off by default. It is for a
compatible condenser microphone connection, not for XPS-30, FM-1, playback
devices, headphones, or computer outputs.

Before enabling it, identify the actual switch, confirm which XLR inputs it
affects, mute or lower the main output, and disconnect any equipment that could
be affected by the same phantom-power bus.

## USB And Bluetooth

USB and Bluetooth are audio features, not MIDI features. Treat USB as a
provisional stereo computer-audio interface until ALSA testing establishes its
capture/playback path. Bluetooth is a convenience playback source and should
not be the only live playback path when reliability matters.
