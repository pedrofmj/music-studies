# Controller Profiles

These are the hardware-side settings verified by the live rack. PipeWire and
Carla cannot reconstruct settings stored inside a controller, so configure
them before diagnosing graph restoration.

## Arturia KeyLab Essential 61 mk3

Use the normal KL Essential 61 mk3 MIDI endpoint on channel 1.

| Control | Messages | Rack role |
| --- | --- | --- |
| Faders 1-9 | CC73, 75, 79, 72, 80, 81, 82, 83, 85 | AR-CH-1 through AR-CH-9 volume |
| Knobs 1-9 | CC74, 71, 76, 77, 93, 18, 19, 16, 17 | AR-CH-1 through AR-CH-9 reverb |
| Central encoder | Relative CC114 | Converted to absolute CC119 master volume |
| Central click | Push CC115 | Toggle the Carla-only stereo mute gate |

Do not route the MCU/HUI or ALV ports into the instrument rack.

## M-VAVE SMK-25

In CubeSuite, use the SMK25-Master endpoint for keys, pads, and knobs. The
transport arrives through the separate AUX capture_2 endpoint.

### Knobs

Set all eight to CC, channel 1, minimum 0, maximum 127:

| Knob | CC | Layer |
| --- | --- | --- |
| 1 | 20 | SMK-CH-1 |
| 2 | 21 | SMK-CH-2 |
| 3 | 22 | SMK-CH-3 |
| 4 | 23 | SMK-CH-4 |
| 5 | 24 | SMK-CH-5 |
| 6 | 25 | SMK-CH-6 |
| 7 | 26 | SMK-CH-7 |
| 8 | 27 | SMK-CH-8 |

### Side-A Pads

Set them to CC Toggle, with values 0 and 127. The MIDI channel is intentionally
different per pad:

| Pad | CC | Channel |
| --- | --- | --- |
| 1 | 40 | 1 |
| 2 | 41 | 2 |
| 3 | 42 | 3 |
| 4 | 43 | 4 |
| 5 | 36 | 5 |
| 6 | 37 | 6 |
| 7 | 38 | 7 |
| 8 | 39 | 8 |

### Transport

Set both buttons to MCP, not CC Toggle.

| Button | MCP message observed | Endpoint |
| --- | --- | --- |
| Stop | Channel-1 Note 93 | AUX capture_2 |
| Play | Channel-1 Note 94 | AUX capture_2 |

Changing transport to CC Toggle changes the MIDI message type and prevents the
SMK layer service's Play/Stop mapping from matching.

## M-VAVE SMC-Mixer

Set faders 1-8 to channel-1 CC40-47 with a 0-127 range. Carla routes each one
through a dedicated SMC-EQ-N CC Scale intermediary and maps it to CC102-109,
controlling 63, 125, 250, 500, 1000, 2000, 4000, and 8000 Hz respectively.

## M-VAVE SMC-PAD And SMC-PAD Pocket

Route SMC-PAD-Master and SMC-PAD Pocket-Master into PD Controls - Sustain
Scale. Their normal note events drive PD-CH-1 - Drum Set. The private endpoints
are not part of the rack.

On SMC-PAD-Master, Knob 1 sends channel-1 CC36 and controls the drum SoundFont
volume through the CC7 mapper. Knob 2 was measured as channel-1 CC37. Both
Master ports feed a dedicated `PD-CH-1 Gain Map` intermediary that converts CC37
to private CC110, controlling post-instrument output gain from 0 to approximately
+12 dB without passing through the sustain-only filter.

## Physical Verification

After configuring a replacement controller, use jack_midi_dump or another raw
MIDI monitor before changing Carla. Verify the exact endpoint, channel, message
type, controller or note number, and press/release values. A controller label
or CubeSuite mode name alone is not enough to prove compatibility.
