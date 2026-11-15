# Controller Mapping Log

Use this directory for the observed configuration, not a desired configuration.
Record every mapping that must be recoverable without relying on memory or the
vendor editor.

| Date | Preset | Bank | Control | MIDI Type | Value/Note/CC | Channel | Host Endpoint | Destination | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-08-08 | Current | Side A | Pads 1-4 | CC toggle | CC40, CC41, CC42, CC43; 127/0 | 1, 2, 3, 4 | SMK25-Master | SMK pad-layer latch 1-4 | Captured | Per-pad MIDI channel |
| 2026-08-08 | Current | Side A | Pads 5-8 | CC toggle | CC36, CC37, CC38, CC39; 127/0 | 5, 6, 7, 8 | SMK25-Master | SMK pad-layer latch 5-8 | Captured | Per-pad MIDI channel |
| 2026-08-08 | Current | Side A | Knobs 1-8 | CC absolute | CC20-CC27 | 1 | SMK25-Master | SMK layer volume 1-8 | Captured | One knob per layer |
| 2026-08-08 | Current | Transport | Stop | Note | 93 | 1 | AUX capture_2 | Stop held chords | Captured | Note On/Off |
| 2026-08-08 | Current | Transport | Play | Note | 94 | 1 | AUX capture_2 | Resume held chords | Captured | Note On/Off |
| 2026-08-08 | Current | Side B | Pad 1 | CC toggle | CC96; 127/0 | 9 | AUX and/or Master | Reserved | Captured | Ignored by latch router |

For a DAW surface, record DAW name, protocol, input endpoint, output endpoint,
and mode. For hardware MIDI out, record the cable or wireless adapter and the
target device. Add a backup entry whenever a mapping changes.
