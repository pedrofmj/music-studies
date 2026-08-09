# Architecture

## Purpose

The Arturia KeyLab Essential central knob sends channel-1 CC114 as relative
steps centered on value 64. Carla's mapped plugin parameters expect absolute
CC values, so directly mapping CC114 causes the master volume to jump around
the midpoint.

The adapter accumulates those relative steps into an absolute value from 0 to
127 and emits that value as channel-1 CC119. The encoder click sends CC115 as
a momentary 127/0 pair; the adapter toggles once on each press and controls a
stereo audio gate after the final EQ. The gate ramps over 10 ms to avoid
clicks. Carla maps CC119 to the LSP mixer's `Output gain`. Fader 9 continues to
emit CC85, but nothing in the current project consumes it.

## Data flow

```text
Arturia MIDI output
  +--> MIDI Scale CC Value --> existing instruments and controls
  |
  +--> Arturia Main Volume Encoder:relative-in
         CC114 relative -> persistent accumulator -> CC119 absolute
         CC115 press -> persistent mute state
       Arturia Main Volume Encoder:absolute-out
         --> LSP Mixer x8 Stereo:events-in -> Output gain

LSP Mixer x8 Stereo:Output L/R
  --> SMC-MIX - 8-Band EQ:Input L/R
      SMC-MIX - 8-Band EQ:Output L/R
        --> Arturia Main Volume Encoder:audio-in-l/r
      persistent mute state -> 10 ms stereo audio gate
      Arturia Main Volume Encoder:audio-out-l/r
        --> system playback L/R
```

The adapter receives only the parallel controller feed and the final stereo
rack output. It is not in the note path, so a service failure does not prevent
the keyboard or other faders from reaching Carla. The audio route intentionally
depends on the service because it owns the final rack-only mute gate.

## Runtime state

The accumulated volume and mute state are stored atomically at:

```text
~/.local/state/arturia-main-volume-encoder/value
```

The file contains `VOLUME MUTE`, where mute is 0 or 127. A legacy file with
only the volume remains valid and starts unmuted. The initial volume fallback
is MIDI value 3, matching the master-gain value saved in the Carla project
when the adapter was introduced. New encoder movement and mute changes are
persisted by the service. Whenever `absolute-out` gains a connection, the
service immediately replays the stored CC119 value so Carla starts at the
persisted main volume without requiring encoder movement.

## Installed files

```text
~/.local/bin/arturia-main-volume-encoder
~/.config/systemd/user/arturia-main-volume-encoder.service
~/.local/state/arturia-main-volume-encoder/value
```

The user service launches the binary through `pw-jack`, ensuring it uses
PipeWire's JACK compatibility layer.

## Patchbay contract

The persistent patchbay snapshot owns these links:

```text
KL Essential 61 mk3 MIDI -> Arturia Main Volume Encoder:relative-in
Arturia Main Volume Encoder:absolute-out -> LSP Mixer x8 Stereo:events-in
LSP Mixer x8 Stereo:Output L/R -> SMC-MIX - 8-Band EQ:Input L/R
SMC-MIX - 8-Band EQ:Output L/R -> Arturia Main Volume Encoder:audio-in-l/r
Arturia Main Volume Encoder:audio-out-l/r -> system playback L/R
```

The adapter service and `pipewire-patchbay-watch.service` are both enabled as
user services on airstar.

## Build and install

From the repository root on a Linux host with GCC, `libjack.so.0`, PipeWire,
and `pw-jack`:

```bash
docs/tools/arturia-main-volume-encoder/install-arturia-main-volume-encoder
```

The source intentionally declares the small JACK ABI surface it uses so it can
build on the current airstar host, which has the runtime library but not the
JACK development headers.

## Portability boundary

The controller transformation itself is platform-neutral: consume relative
CC114, maintain a clamped accumulator, emit absolute CC119, and turn CC115
presses into persistent stereo mute state. The current JACK MIDI/audio client,
systemd service, and PipeWire connections are Linux adapters.

A future Echora/Galaxy implementation should preserve that transformation and
state contract while replacing device discovery, MIDI transport, persistence,
and service lifecycle with the corresponding SDK facilities.
