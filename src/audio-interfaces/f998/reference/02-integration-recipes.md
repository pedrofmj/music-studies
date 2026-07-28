# Integration Recipes

## Voice Stream With Computer

```text
microphone -> F998 confirmed microphone input
F998 OTG/PC port -> Ubuntu computer after USB endpoint verification
headphones -> F998 confirmed monitoring output
```

Begin with dry voice. Add background music and effects one at a time. Record
whether the USB capture includes monitoring, Bluetooth, and effects.

## XPS-30 And FM-1 Worship Audio

```text
XPS-30 OUTPUT L/R -> TEYUN A8 line inputs
FM-1 headphone out -> stereo breakout -> TEYUN A8 line inputs
TEYUN A8 MAIN OUT -> PA or powered speakers
```

This is the correct audio path for instruments. Do not place F998 between the
instruments and PA without a documented, low-level test of its actual input and
output topology.

## Add Spoken Voice To A Stream

```text
spoken microphone -> F998 -> computer/phone streaming path
instrument audio -> TEYUN A8 -> PA/room sound
```

Keep these paths independent until a verified feed from the A8 to F998 is
needed for online listeners. If adding that feed, use an attenuated, low-level
test into the confirmed F998 accompaniment input and check for feedback.
