# Connection Recipes

Use the capability records as the authority for each device. These recipes are
valid only when every device has power and every required cable or adapter has
been identified and tested.

## Signal Rules

- **MIDI is control data, not sound.** A MIDI controller needs a sound source;
  a sound source still needs an audio path to a mixer, PA, audio interface, or
  headphones.
- **USB MIDI devices need a host.** Do not cable two USB device ports together.
  Use a computer, mobile device, or proven USB-MIDI host interface.
- **Audio and MIDI can travel independently.** A controller can play the
  XPS-30 through MIDI while the XPS-30 sound travels to the PA through OUTPUT.
- **A connector shape does not establish wiring.** Verify the adapter type and
  polarity for every 3.5 mm or 1/4-inch MIDI route before relying on it.

## Port Matrix

| Device | Creates sound | Direct hardware MIDI path | Host MIDI path | Audio path |
| --- | --- | --- | --- | --- |
| XPS-30 | Yes | MIDI IN and MIDI OUT connectors | USB COMPUTER MIDI | OUTPUT R/L-MONO, PHONES, USB audio to DAW |
| SMK25 | No | Reconfigured pedal port MIDI OUT or optional wireless adapter | USB-B or BLE MIDI | None |
| SMC-PAD | No | 3.5 mm MIDI OUT or optional wireless adapter | USB-C or BLE MIDI | None |
| SMC-PAD Pocket | No | Optional wireless five-pin adapter only | USB-C or BLE MIDI | None |
| SMC-Mixer | No | Optional wireless MIDI device only | USB-C or BLE MIDI | None |
| FM-1 | Yes | 3.5 mm MIDI **IN** only | USB-C or BLE MIDI | 3.5 mm stereo headphone output and internal speaker |
| TEYUN A8 | No | None documented | USB audio only, pending verification | MAIN OUT and headphone outputs, pending physical confirmation |
| F998 Live Sound Card | No | None documented | Claimed USB/OTG audio, pending verification | Consumer monitoring and streaming outputs; not a PA main output |

The device-specific source evidence and constraints are in the linked
[device index](devices.json).

## 1. XPS-30 Alone To A PA

```text
XPS-30 OUTPUT L/MONO  -> mixer or PA mono input
XPS-30 OUTPUT L + R   -> mixer stereo inputs
Roland DP-10          -> XPS-30 PEDAL HOLD
```

Use L/MONO by itself for a mono sound system. Use both outputs for a stereo
system. The DP-10 belongs in PEDAL HOLD; PEDAL CONTROL is the separate
assignable input. See [XPS-30 capabilities](../instruments/roland-xps30/reference/capabilities.json).

## 2. Computer-Centered Recording Or Software-Instrument Setup

```text
SMK25 / SMC-PAD / SMC-PAD Pocket / SMC-Mixer / FM-1
  -> individual USB or BLE MIDI connections -> computer MIDI host

XPS-30 -> USB COMPUTER MIDI/audio -> computer
FM-1   -> 3.5 mm stereo audio -> audio interface input
XPS-30 OUTPUT -> audio interface line inputs when not using USB audio
audio interface -> monitors / mixer / PA
```

The computer is the MIDI host, routing point, and DAW endpoint. Keep each
controller's target, MIDI channel, notes, and control-surface protocol distinct.
The SMC-Mixer is useful here as the DAW control surface; it does not process
audio.

For Ubuntu, first record the ALSA endpoint names with `aconnect -l` and
`amidi -l`. The host-specific mapping belongs in that device's `mappings/`
directory, not in its manual facts.

## 3. SMC-PAD Directly Controls XPS-30

```text
SMC-PAD 3.5 mm MIDI OUT
  -> compatible 3.5 mm MIDI-to-XPS-30 MIDI-IN adapter
  -> XPS-30 MIDI IN

XPS-30 OUTPUT -> mixer / PA / headphones
```

This is the documented direct wired controller route. The manual does not
specify the adapter wiring standard in the stored text, so record the exact
adapter and perform a note test before using it on stage. Map pad notes away
from XPS-30 phrase-pad or external-device collisions.

## 4. SMK25 Directly Controls XPS-30

```text
SMK25 pedal port, changed to MIDI OUT in vendor editor
  -> verified 1/4-inch MIDI adapter
  -> XPS-30 MIDI IN

XPS-30 OUTPUT -> mixer / PA / headphones
```

This route sacrifices the SMK25's pedal input. Do not change the port role on
the day of a performance. If sustain is necessary, use the XPS-30 keyboard and
its DP-10 instead, or move the SMK25 to a computer-hosted setup.

## 5. SMC-PAD Pocket To A Hardware Sound Module

```text
SMC-PAD Pocket -> optional M-VAVE five-pin wireless MIDI adapter -> XPS-30 MIDI IN
XPS-30 OUTPUT -> mixer / PA / headphones
```

The manual documents this optional wireless route and says it prevents the
Pocket from connecting to another host. Treat it as a single-destination setup.
There is no documented wired MIDI-OUT jack on the Pocket.

## 6. FM-1 As A Second Live Sound Source

```text
FM-1 headphone output -> stereo breakout / suitable audio-interface or mixer input
XPS-30 OUTPUT          -> separate mixer or audio-interface inputs
mixer / interface      -> PA or monitors
```

The FM-1's audio path is independent from its MIDI path. It can receive notes
via USB-C MIDI, BLE MIDI, or 3.5 mm MIDI IN, but its physical 3.5 mm MIDI port
is not a MIDI output. To play the XPS-30 from FM-1 keys, route FM-1 USB/BLE
MIDI through a host that sends MIDI to XPS-30 MIDI IN or USB COMPUTER.

## 7. SMC-Mixer In A Live Or Recording Rig

```text
SMC-Mixer -> USB-C or BLE MIDI -> computer DAW
computer  -> audio interface -> monitors / mixer / PA
```

Configure SMC-Mixer as both input and output for Mackie Control in the DAW.
Use an actual audio mixer or interface for XPS-30 and FM-1 sound. The
SMC-Mixer's faders control DAW parameters only.

## Not Documented As Direct Connections

| Proposed route | Why it is not a documented direct plan |
| --- | --- |
| SMK25 USB-B -> XPS-30 USB COMPUTER | Both require a USB host. |
| SMC-PAD USB-C -> XPS-30 USB COMPUTER | Both require a USB host. |
| SMC-PAD Pocket USB-C -> XPS-30 USB COMPUTER | Both require a USB host. |
| FM-1 physical MIDI port -> XPS-30 MIDI IN | FM-1 physical MIDI connector is MIDI IN, not OUT. |
| Any controller -> SMC-Mixer -> PA | SMC-Mixer has no audio input or output. |
| Any MIDI connection -> audible FM-1 or XPS-30 sound | MIDI needs a separate audio connection or documented USB-audio route. |


## 8. TEYUN A8 As The Audio Hub

```text
XPS-30 OUTPUT L/R -> two confirmed A8 line inputs
FM-1 headphone out -> 3.5 mm TRS-to-two-mono breakout -> two confirmed A8 line inputs
A8 MAIN OUT L/R -> powered speakers, power amplifier, or PA line inputs
A8 USB audio -> Ubuntu computer only after endpoint and capture-path verification
```

The A8 receives and mixes audio; it does not replace any MIDI connection. Keep
controller-to-synth MIDI routing separate from these audio cables. Keep phantom
power off around XPS-30, FM-1, phones, and computer outputs. A source-managed
A8 integration and its current uncertainties are in
[TEYUN A8 capabilities](../audio-interfaces/teyun-a8/reference/capabilities.json).


## 9. F998 For Streaming Voice, Not Instrument Mixing

```text
voice microphone -> F998 confirmed microphone input
F998 OTG/PC port -> computer after endpoint verification
headphones -> F998 confirmed monitor output

XPS-30 + FM-1 -> TEYUN A8 -> PA or powered speakers
```

F998 is an audio processor/streaming interface, not a MIDI device and not the
preferred place to combine keyboard line outputs. Keep XPS-30 and FM-1 on the
TEYUN A8. Feed any A8 mix into F998 only after confirming F998 accompaniment
input labels, headroom, cable wiring, and feedback-free monitoring. Details are
in [F998 capabilities](../audio-interfaces/f998/reference/capabilities.json).
