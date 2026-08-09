# Airstar MIDI Setup

This directory stores the captured Linux workstation graph for the Carla live
rack on airstar. The current reference was verified on 2026-08-08 and is paired
with the complete deployment tooling under
[docs/tools/airstar-live-setup](../../docs/tools/airstar-live-setup/README.md).

## Artifacts

- [Current State](current-state.md) summarizes the verified host, rack, devices,
  services, and graph.
- [Operating Procedure](operating-procedure.md) covers startup, snapshot
  refresh, validation, recovery, and capture.
- [Plugin Inventory](plugin-inventory.md) records package and manual plugin
  ownership.
- [Raw Patchbay](airstar-patchbay.json) contains 112 links exactly as captured
  on airstar.
- [MIDI Patchbay](airstar-midi-patchbay.json) contains 64 MIDI links and their
  endpoints.
- [Deployment Patchbay](pedro-live-rack-patchbay.json) contains the 110 links
  owned by the performance setup. It excludes two unrelated Java playback
  links from the shared system sink.

The raw snapshots are evidence. The deployment snapshot, Carla project,
setup.json manifest, and helper service sources are the rebuild inputs.

## Sources Of Truth

- Carla plugin instances, parameters, rack order, and Carla-owned connections:
  src/audio-software/carla/projects/pedro-live-rack/pedro.uproject
- External PipeWire links and automatic restoration:
  ~/.local/state/pipewire-patchbay/patchbay.json on the installed machine
- Controller messages, packages, external asset fingerprints, and expected
  service state: docs/tools/airstar-live-setup/setup.json
- Stateful SMK chord/layer logic and Arturia master volume/mute behavior:
  docs/tools/smk25-pad-layers and docs/tools/arturia-main-volume-encoder

SoundFonts, DecentSampler binaries, and sample libraries remain external and
are verified by checksum rather than committed to Git.
