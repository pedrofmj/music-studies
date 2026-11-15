# Controller Mapping Log

Use this directory for the observed configuration, not a desired configuration.
Record every mapping that must be recoverable without relying on memory or the
vendor editor.

| Date | Preset | Bank | Control | MIDI Type | Value/Note/CC | Channel | Host Endpoint | Destination | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-08-11 | Default - Pedro Rack Setup | Current power-on bank | Control Pads 1-8 | Hardware internal | Note Repeat, Latch, Rate -, Rate +, Swing -, Swing +, Bank -, Bank + | - | Hardware internal | Controller state | Captured | Operator confirmed; all eight presses produced no MIDI event |
| 2026-08-11 | Default - Pedro Rack Setup | Current power-on bank | Performance Pads 9-16 | Note | 40, 41, 42, 43, 36, 37, 38, 39 | 10 | SMC-PAD Pocket-Master | PD-CH-1 Drum Set | Captured | One ordered note-on/off pair per pad; note-off velocity 64 |

For a DAW surface, record DAW name, protocol, input endpoint, output endpoint,
and mode. For hardware MIDI out, record the cable or wireless adapter and the
target device. Add a backup entry whenever a mapping changes.
