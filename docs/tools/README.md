# Music Tools

This directory contains small operational tools used by the music setup. Each
tool keeps its implementation, installer, service definitions, and operating
documentation together so it can be understood or replaced independently.

## Current tools

- [Arturia Main Volume Encoder](arturia-main-volume-encoder/README.md) converts
  the KeyLab Essential central knob's relative MIDI messages into an absolute
  master-volume control while leaving fader 9 available for an instrument.
- [SMK-25 Pad Layers](smk25-pad-layers/README.md) provides eight independently
  latchable Carla instrument layers with per-knob volume and Play/Stop recall.

## Future direction

The current tools are pragmatic host-specific implementations. They do not yet
depend on Echora or Galaxy.

When those ecosystems and their SDKs are ready for this use case, these tools
should migrate behind portable SDK abstractions for:

- MIDI device and port discovery
- controller event transformation and persistent state
- graph connection management
- process and service lifecycle management
- platform-specific audio/MIDI backends

The externally visible behavior and configuration should remain stable while
Linux/PipeWire/JACK-specific details move into backend adapters. This keeps the
tools usable now without committing the long-term implementation to one
platform.
