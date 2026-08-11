# Controller Mapping Log

Use this directory for the observed configuration, not a desired configuration.
Record every mapping that must be recoverable without relying on memory or the
vendor editor.

| Date | Preset | Bank | Control | MIDI Type | Value/Note/CC | Channel | Host Endpoint | Destination | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-08-11 | Default - Pedro Rack Setup | Current power-on bank | Performance Pads 1-16 | Note | 48, 49, 50, 51, 44, 45, 46, 47, 40, 41, 42, 43, 36, 37, 38, 39 | 10 | SMC-PAD-Master | PD-CH-1 Drum Set | Captured | One ordered note-on/off pair per pad; note-off velocity 64 |
| 2026-08-11 | Default - Pedro Rack Setup | Current knob bank | Knob 1 | CC | CC36 | 1 | SMC-PAD-Master | PD-CH-1 Volume Map | Captured | Protected setup evidence |
| 2026-08-11 | Default - Pedro Rack Setup | Current knob bank | Knob 2 | CC | CC37 | 1 | SMC-PAD-Master | PD-CH-1 Output Gain 0 to +12 dB | Captured | Protected setup evidence |

For a DAW surface, record DAW name, protocol, input endpoint, output endpoint,
and mode. For hardware MIDI out, record the cable or wireless adapter and the
target device. Add a backup entry whenever a mapping changes.
