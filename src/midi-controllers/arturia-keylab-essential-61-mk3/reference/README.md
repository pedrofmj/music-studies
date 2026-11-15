# KeyLab Essential 61 mk3 Reference

[Structured capabilities](capabilities.json) records source-linked ports,
signal directions, constraints, and supported roles. Manual claims are kept
separate from local testing.

## Architecture

\`\`\`text
keys / pads / wheels / encoders / faders / pedal
  -> Arturia, DAW, or User program
  -> USB-C MIDI or five-pin MIDI OUT
  -> software instrument, DAW, or hardware sound module
  -> separate audio path from the sound source
\`\`\`

The KeyLab is a controller, not a sound source. Its USB-C connection carries
power, MIDI data, and control information. Its five-pin MIDI OUT controls a
hardware synth directly, while the USB DINTHRU port routes host-generated MIDI
out of that same physical jack.

## Controls And Programs

| Surface | Practical Use |
| --- | --- |
| 61 keys | Notes and velocity for software instruments or hardware modules |
| Pitch and modulation wheels | Performance expression; confirm each target's assignments |
| Eight pads | Velocity- and aftertouch-sensitive note or custom assignments |
| Nine encoders and nine faders | Analog Lab macros, DAW parameters, or User-program MIDI controls |
| DAW command controls | Transport, track, and parameter control with a dedicated DAW integration or MCU/HUI |
| 1/4-inch TRS pedal input | Sustain, switch, or continuous/expression control after the pedal is tested |

Use \`Prog\` to select the intended program before diagnosing a mapping:
Arturia for Analog Lab integration, DAW for supported DAW scripts, and a User
program for custom assignments. Back up User programs in MIDI Control Center
before firmware changes or a factory reset.

## Connection Recipes

### USB To A Software Instrument Or DAW

1. Connect the KeyLab USB-C port to the computer and confirm the standard MIDI
   endpoint.
2. Route that endpoint to one simple software instrument and confirm notes,
   velocity, pitch, modulation, one pad, one encoder, and one fader.
3. Enable the dedicated DAW integration only after this note path works.
4. Record every observed endpoint and selected program in
   [Mappings](../mappings/README.md).

The Arturia manual calls the standard endpoint \`KL Essential 61 mk3 MIDI\`.
Linux may use a different visible ALSA client and port name, so record what the
host reports rather than copying the vendor label into a tested mapping.

### Direct KeyLab To Roland XPS-30

\`\`\`text
KeyLab Essential MIDI OUT -> five-pin MIDI cable -> XPS-30 MIDI IN
XPS-30 OUTPUT L/MONO     -> mixer / PA / headphones
\`\`\`

Set the KeyLab MIDI transmit channel and XPS-30 receive part deliberately,
then test a plain XPS-30 piano patch before assigning pads, pedals, program
changes, or DAW functions. The MIDI cable carries control data only; it does
not replace the XPS-30 audio connection.

### Host Sequencing To Hardware MIDI

\`\`\`text
computer DAW -> KeyLab USB DINTHRU endpoint -> KeyLab five-pin MIDI OUT
             -> XPS-30 MIDI IN
\`\`\`

Use this route when the computer needs to sequence the XPS-30 through the
KeyLab. It is separate from the keyboard's standard MIDI endpoint. Avoid MIDI
feedback by selecting one intentional input and one intentional output in the
DAW.

### DAW Control

Arturia provides integrations for Ableton Live, Bitwig Studio, Cubase, FL
Studio, and Logic Pro. For other DAWs, configure either MCU or HUI in MIDI
Control Center and use the dedicated \`MCU/HUI\` port for the control surface;
keep note input on the standard MIDI port. Do not enable the MCU/HUI port when
it is not being used because transport controls can otherwise produce unwanted
MIDI events.

## Linux Notes

USB is the first connection to test. On the host, inspect MIDI endpoints with
\`aconnect -l\` and \`amidi -l\`, then save the exact names in the mapping log.
The manual describes the keyboard as class-compliant, but it does not document
Linux-specific port names, MIDI Control Center support, or every virtual-port
behavior. Treat those as observed facts, not guarantees.

Do not use a USB-C-to-USB-C cable between the KeyLab and the XPS-30 USB
COMPUTER port: both require a USB host. Use the KeyLab five-pin MIDI OUT to
the XPS-30 MIDI IN for the direct hardware route.
