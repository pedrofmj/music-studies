# Architecture

## Purpose

The Arturia KeyLab Essential central knob sends channel-1 CC114 as relative
steps centered on value 64. Carla's mapped plugin parameters expect absolute
CC values, so directly mapping CC114 causes the master volume to jump around
the midpoint.

The adapter accumulates those relative steps into an absolute value from 0 to
127 and emits that value as channel-1 CC119. Carla maps the LSP mixer's
`Output gain` parameter to CC119. Fader 9 continues to emit CC85, but nothing
in the current project consumes it.

## Data flow

```text
Arturia MIDI output
  +--> MIDI Scale CC Value --> existing instruments and controls
  |
  +--> Arturia Main Volume Encoder:relative-in
         CC114 relative -> persistent accumulator -> CC119 absolute
       Arturia Main Volume Encoder:absolute-out
         --> MIDI Scale CC Value --> LSP Mixer x8 Stereo:Output gain
```

The adapter receives only the parallel controller feed. It is not in the note
path, so a service failure does not prevent the keyboard from playing notes or
the other faders from working.

## Runtime state

The accumulated value is stored atomically at:

```text
~/.local/state/arturia-main-volume-encoder/value
```

The initial fallback is MIDI value 3, matching the master-gain value saved in
the Carla project when the adapter was introduced. New encoder movement is
persisted by the service.

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
Arturia Main Volume Encoder:absolute-out -> MIDI Scale CC Value:events-in
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

The MIDI transformation itself is platform-neutral: consume relative CC114,
maintain a clamped accumulator, and emit absolute CC119. The current JACK
client, systemd service, and PipeWire connections are Linux adapters.

A future Echora/Galaxy implementation should preserve that transformation and
state contract while replacing device discovery, MIDI transport, persistence,
and service lifecycle with the corresponding SDK facilities.
