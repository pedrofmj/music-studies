# Documented Capabilities

Sources: [LMMS user manual](https://docs.lmms.io/user-manual/) and
[Using MIDI](https://docs.lmms.io/user-manual/en/production/midi).

LMMS is documented in this repository as a pattern-oriented composition and
software-instrument environment. It is not a claim that a particular interface,
controller, plugin, or export format works on a local machine.

## MIDI

- The manual recommends ALSA Sequencer for Linux MIDI settings.
- The Windows default documented in the manual is WinMM MIDI.
- A MIDI device can be selected quickly for an instrument, while the Instrument
  Editor MIDI tab provides more detailed routing controls.
- Controller assignments use the target control's controller-connection action
  and automatic detection, then require a physical control movement.

Record the actual backend, endpoint name, MIDI channel, KeyLab program, and
destination in the LMMS device-integration checklist. Do not copy a vendor
endpoint label into a local result without observing it.

## Project Workflow

LMMS provides pattern, instrument, piano-roll, automation, mixer, and controller
work areas. Treat each project as recoverable only after it has been saved,
reopened, and produced sound through the intended output route.

## Boundaries

- LMMS receives and sequences MIDI; it is not an audio interface or a hardware
  MIDI host by itself.
- Audio output, latency, plugin availability, and controller mapping depend on
  the selected backend, OS, installed packages, and current project.
- The direct KeyLab-to-XPS-30 route does not involve LMMS. Use the KeyLab
  five-pin MIDI OUT and route XPS-30 audio separately.
