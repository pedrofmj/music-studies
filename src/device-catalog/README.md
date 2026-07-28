# Device Catalog

This directory is the connection-planning entry point for the hardware in this
repository. It answers what a device can generate, receive, route, and control
without mixing up MIDI, audio, USB storage, power, or wireless transport.

## Use This Catalog

1. Open [devices.json](devices.json) to locate a device capability record.
2. Read the device's `reference/capabilities.json` for machine-searchable
   ports, roles, constraints, and source references.
3. Use [connection-recipes.md](connection-recipes.md) for documented working
   topologies and requirements.
4. Consult the device's `manual/extracted/` text for exhaustive manual search.

The raw text is a local search layer for the retained official PDFs. The
capability records are curated connection facts. A `manual_only` or `unknown`
field means that the repository does not yet have enough evidence to claim a
specific behavior.

## Search

Search the structured records first:

```bash
rg -n 'midi_out|audio_outputs|usb_computer|BLE|host_required' \
  src/device-catalog src/instruments src/midi-controllers
```

Search all extracted manual text when a detail is missing:

```bash
rg -n -i 'MIDI OUT|USB audio|pedal polarity|Bluetooth' \
  src/**/manual/extracted
```

Do not treat matching text alone as a proven wiring plan. Check connector type,
signal direction, device role, and every required adapter before connecting
hardware.

## Contents

- [Schema](schema.md) - fields and evidence rules for capability records.
- [Device index](devices.json) - all documented hardware and its records.
- [Connection recipes](connection-recipes.md) - repeatable setups and limits.
