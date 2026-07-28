# PipeWire Learning Path

These sessions apply to Linux only. Record distribution, PipeWire version,
session manager, connected devices, application versions, and result in
reference/ after each test.

## 01. Establish A Linux Baseline

Confirm the active PipeWire service through the distribution tooling. Record
PipeWire version, session manager, current audio output, current audio input,
and any connected USB audio interface.

Success condition: the selected audio output produces intentional test audio and
the baseline record identifies the active device names.

## 02. Verify Client Compatibility

Start one native audio client, then test the intended client path for LMMS and
Carla separately. Record which application starts, which ports appear, and
whether audio reaches the selected output.

Success condition: each tested client has a recorded start result and a clear
audio route or a documented failure.

## 03. Route One Device

Connect one audio interface and one MIDI controller. Verify each independently,
then record the graph connections required for a simple instrument or audio
playback path.

Success condition: the selected application receives the intended MIDI event
when applicable and monitored audio reaches the chosen interface output.

## 04. Reopen And Recheck

Restart the tested applications and reconnect the hardware only as needed.
Compare the observed device and port names with the previous record.

Success condition: the required route can be rebuilt from the documented
evidence without relying on memory.
