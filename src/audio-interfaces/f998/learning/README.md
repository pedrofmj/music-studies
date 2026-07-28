# F998 Learning

Use this sequence before connecting it to instruments, a PA, or a live stream.
The F998 layout varies by reseller; record the exact labels in
[Routing](../routing/README.md).

## 1. Confirm The Physical Revision

1. Photograph the front, rear, and product label.
2. Record every jack label and connector size.
3. Confirm the charging port, computer/OTG port, condenser-microphone input,
   accompaniment input, headset/earphone outputs, and live-stream ports.
4. Compare the label with FCC ID `2A6NU-F998` if present.

## 2. Microphone And Headphone Baseline

1. Start with MIC, RECORD, MONITOR, ECHO, and backing-track controls low.
2. Connect only the microphone type the physical port accepts.
3. Connect headphones before any streaming device.
4. Speak at the loudest expected level, raise MIC gradually, then set MONITOR.
5. Add echo and voice processing only after a clean dry signal is confirmed.

## 3. Accompaniment And Bluetooth

1. Test one wired accompaniment source at low level.
2. Test Bluetooth as a separate source, not as a live-critical replacement.
3. Record which source reaches headphones and which source reaches a phone or
   computer stream.
4. Keep backing-track level below the microphone in monitoring to avoid masking.

## 4. USB/OTG Test On Ubuntu

1. Connect the documented computer/OTG port with a data cable.
2. Run `arecord -l`, `aplay -l`, and `wpctl status`.
3. Record capture and playback endpoint names, channel count, and sample rates.
4. Make a short voice recording and verify that computer playback does not form
   a feedback loop into capture.
5. Test only one output destination at first: headphones, computer, or a phone.

Do not connect XPS-30 or FM-1 at normal line/headphone level until the F998
input type and headroom have been observed. Use the TEYUN A8 for those sources
in the meantime.
