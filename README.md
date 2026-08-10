# Music Studies

This repository stores structured study material for instruments, MIDI
controllers, live setup, sound design, and reproducible practice workflows.

The proposed Configurable Performance Rig
([English](docs/features/0001.0000.0000.0000-configurable-performance-rig/configurable-performance-rig.md),
[Português (Brasil)](docs/features/0001.0000.0000.0000-configurable-performance-rig/configurable-performance-rig.pt-BR.md))
defines the next-step concepts for global and per-device profiles,
low-latency CLI switching, and future MIDI-triggered profile changes. The
[implementation plan](docs/features/0001.0000.0000.0000-configurable-performance-rig/configurable-performance-rig-implementation-plan.md)
defines the architecture, milestones, validation gates, multiplatform
delivery, deployment, and rollback.

Progress is maintained in the feature's
[visual tracker](docs/features/0001.0000.0000.0000-configurable-performance-rig/configurable-performance-rig-tracker.md).

## Current Collections

- [Roland XPS-30](src/instruments/roland-xps30/README.md) - synthesizer,
  worship setup, sound design, and Linux workflow study.
- [MIDI Controllers](src/midi-controllers/README.md) - Arturia and M-VAVE controller studies, mappings, DAW-control workflows, and
  Linux verification notes.
- [Audio Interfaces And Mixers](src/audio-interfaces/README.md) - analog
  routing, USB audio, monitoring, and mixer integration studies.
- [Airstar MIDI Setup](src/midi-setup/README.md) - verified PipeWire, Carla,
  controller, plugin, and recovery notes for the Linux music workstation.
- [Device Catalog](src/device-catalog/README.md) - source-linked capabilities,
  port matrix, and connection recipes across all documented hardware.
- [Audio Software](src/audio-software/README.md) - Linux and Windows software
  inventory, licensing, functions, and manual-intake structure.

## Structure

- `src/instruments/<instrument>/learning/` - ordered hands-on study sessions.
- `src/instruments/<instrument>/reference/` - explanations, recipes, and
  lookup material.
- `src/instruments/<instrument>/manual/` - official manual inventory and
  locally retained sources where authorized.
- `src/midi-controllers/<controller>/learning/` - controller exercises and
  connection checks.
- `src/midi-controllers/<controller>/reference/` - controller architecture,
  setup recipes, and Linux guidance.
- `src/midi-controllers/<controller>/mappings/` - observable MIDI, DAW, and
  preset assignments.
- `src/midi-controllers/<controller>/backups/` - recovery evidence and restore
  notes.
- `src/device-catalog/` - cross-device capabilities, connection constraints,
  and evidence-backed setup recipes.

The repository keeps practical study under version control: what was learned,
what was configured, what changed, and how to rebuild it.
