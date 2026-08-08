# SMK-25 Pad Layers

SMK-25 Pad Layers is a stateful JACK/PipeWire MIDI router for eight Carla
instrument layers controlled by the original M-VAVE SMK-25.

## Behavior

- Side-A Pads 1-8 enable or disable continuous hold for layers 1-8.
- Knobs 1-8 retain their distinct CC20-27 identities and are routed only to
  their corresponding layer and mixer gain.
- Keys and other performance messages are fanned out to all eight layers.
- A disabled layer follows normal Note On and Note Off behavior.
- An enabled layer holds the latest keyboard gesture after key release.
- The first Note On after all physical keys are released starts a new gesture,
  releases the previous chord, and records the replacement chord.
- Stop releases all layer notes but retains each enabled layer's last chord.
- Play resumes those chords. A layer without history starts C3-E3-G3.
- Disabling a layer releases and forgets its chord.

Standard MIDI Start, Continue, Stop, and MMC Play/Stop are recognized
automatically. Device-specific Play and Stop messages can be added in the
mapping file.

## Mapping File

The file smk25-pad-layers.conf records observed hardware messages. Pads can use
two-value CC state (pad_behavior=value) or press-to-toggle Note/CC messages
(pad_behavior=toggle). Do not deploy guessed mappings: capture them with
`jack_midi_dump` first.

Side-A controls must be distinguishable from keyboard notes. CC-toggle pads
are preferred. If the pads send Note messages on the keyboard's channel, use
the M-VAVE editor to assign them unique CCs or a separate MIDI channel.

### Observed Controls

| Control | Message | Endpoint |
| --- | --- | --- |
| Side-A Pads 1-4 | CC40-43 on channels 1-4, values 127/0 | SMK25-Master |
| Side-A Pads 5-8 | CC36-39 on channels 5-8, values 127/0 | SMK25-Master |
| Knobs 1-8 | CC20-27 on channel 1 | SMK25-Master |
| Stop | Note 93 on channel 1 | AUX `capture_2` |
| Play | Note 94 on channel 1 | AUX `capture_2` |
| Side-B Pad 1 | CC96 on channel 9, values 127/0 | Separate from Side-A; ignored |

Captured on `airstar` on 2026-08-08. The remaining Side-B pads are reserved and
are not consumed by this service.

The controller's local two-color toggle can display latch state. The retained
manual does not document inbound RGB feedback, so automatic LED resynchronizing
after a controller or service restart remains a separate hardware test.

## Data Flow

~~~text
SMK25-Master (keys, Side-A pads, knobs) --+
                                           +-> SMK25 Pad Layers:midi-in
SMK25 AUX capture_2 (Stop and Play) --------+
       -> layer-1 -> SMK-CH-1 instrument + SMK layer mixer events-in
       -> layer-2 -> SMK-CH-2 instrument + SMK layer mixer events-in
       ...
       -> layer-8 -> SMK-CH-8 instrument + SMK layer mixer events-in

SMK-CH-N instrument audio -> SMK layer mixer channel N -> master mixer
~~~

The router preserves each physical knob CC and sends the layer to its Carla
instrument and the shared SMK layer mixer. The mixer maps CC20-27 to its eight
channel gains; the named router outputs are stable patchbay intermediaries.

## State

The service atomically stores enabled flags, paused flags, last chords, and the eight most recent knob values at:

~~~text
~/.local/state/smk25-pad-layers/state
~~~

Restarting the service never starts audio automatically. Enabled layers load
paused and resume only after Play or a new keyboard gesture.

## Build And Install

The installer compiles against the JACK runtime ABI, runs the offline
self-test, validates the mapping file, installs the mapping and systemd unit,
and starts the user service:

~~~bash
docs/tools/smk25-pad-layers/install-smk25-pad-layers
~~~

The source intentionally uses the same header-free JACK ABI approach as the
Arturia main-volume service on airstar.

## Portability Boundary

Chord replacement, per-layer state, transport semantics, and the mapping
configuration are backend-neutral. JACK, PipeWire, and systemd are the current
Linux adapters. A future Echora/Galaxy implementation should retain this
behavior while replacing device discovery, MIDI transport, and service
lifecycle through the SDKs.
