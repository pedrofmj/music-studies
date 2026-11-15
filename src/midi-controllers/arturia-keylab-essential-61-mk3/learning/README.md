# KeyLab Essential 61 mk3 Learning Path

Use the reference for lookup; use this file to establish a repeatable working
setup. Record observations as they happen rather than recreating them later.

## Session 1: Arrival Baseline

1. Record the serial label, firmware revision, supplied cable, software bundle
   status, and external condition.
2. Photograph the default control layout and rear panel before changing a User
   program or firmware.
3. Inspect the control-pedal jack, five-pin MIDI OUT, and USB-C connector.
4. Add the baseline to [Backups](../backups/README.md) and retain the photos
   outside the repository if they include personal registration data.

Done when the as-received state is documented.

## Session 2: USB MIDI On Linux

1. Connect the KeyLab directly with its USB-C data cable.
2. Run \`aconnect -l\` and \`amidi -l\`; record every observed KeyLab endpoint.
3. Route the standard MIDI endpoint to one plain software instrument.
4. Test the full keyboard range, pitch wheel, modulation wheel, one pad, one
   encoder, and one fader.
5. Record the selected \`Prog\` mode, MIDI channel, endpoint, and destination in
   [Mappings](../mappings/README.md).

Done when a USB note path is repeatable after reconnecting the keyboard.

## Session 3: Direct Roland XPS-30 Control

1. Power the KeyLab by USB-C and connect its five-pin MIDI OUT to XPS-30 MIDI
   IN with a standard MIDI cable.
2. Start with KeyLab channel 1 and a simple XPS-30 piano patch; adjust the
   XPS-30 receive part only if the note test fails.
3. Confirm note range, velocity, pitch, modulation, and sustain separately.
4. Connect the XPS-30 OUTPUT to headphones, mixer, or PA and set safe gain.
5. Log the cable, channels, XPS-30 patch or performance, and pedal behavior.

Done when the XPS-30 produces sound from the KeyLab with no computer involved.

## Session 4: DAW Control And Analog Lab

1. First confirm the standard MIDI note path, then choose Arturia, DAW, or a
   User program deliberately.
2. Use Arturia mode for Analog Lab, with the standard MIDI and Analog Lab
   display ports selected as observed on the host.
3. For a supported DAW, install the applicable Arturia integration and select
   DAW mode before testing transport, track navigation, faders, and encoders.
4. For an unsupported DAW, use either MCU or HUI in MIDI Control Center and
   configure the dedicated MCU/HUI port as the control surface.
5. Keep the standard MIDI note input separate from the control-surface input.

Done when notes, transport, and fader feedback work without duplicate notes or
MIDI feedback.

## Session 5: User Programs And Recovery

1. Export or otherwise preserve the working state in MIDI Control Center.
2. Create one conservative User program only after the default behavior is
   recorded.
3. Log every changed control, channel, and target in Mappings.
4. Add the export location, firmware revision, and a restore test result to
   Backups.

Do not start a firmware update until a recovery record exists.
