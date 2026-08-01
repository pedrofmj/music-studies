# Documentation Coverage

Reviewed: 2026-08-01.

This matrix makes the study system's evidence boundary explicit. A documented
fact has a source named in the applicable manual manifest or capability record.
An observed fact has a dated local test. A provisional fact is never a basis for
a live or safety-critical route.

## Hardware

| Device | Source Coverage | Operational Coverage | Remaining Evidence |
| --- | --- | --- | --- |
| [Roland XPS-30](../instruments/roland-xps30/README.md) | Six retained Roland manuals, including MIDI implementation and parameter guide | capabilities, Linux workflow, live setup, learning, and backups | Record actual USB endpoint, firmware, and performance mappings |
| [Arturia KeyLab Essential 61 mk3](../midi-controllers/arturia-keylab-essential-61-mk3/README.md) | Retained official 53-page manual and current Arturia downloads source | USB, direct five-pin XPS-30, DINTHRU, DAW, and recovery procedures | Record firmware, ALSA ports, pedal behavior, and User programs |
| [M-VAVE SMK25](../midi-controllers/mvave-smk25/README.md) | Retained vendor manual | USB, BLE limitations, hardware MIDI role change, mappings, and backups | Record actual endpoint, adapter wiring, and preset state |
| [M-VAVE SMC-Mixer](../midi-controllers/mvave-smc-mixer/README.md) | Retained vendor manual | DAW-control role, capabilities, mappings, and backups | Record DAW protocol and bidirectional endpoint results |
| [M-VAVE SMC-PAD](../midi-controllers/mvave-smc-pad/README.md) | Retained vendor manual | USB, BLE, direct MIDI, mappings, and backups | Record adapter wiring and actual pad assignments |
| [M-VAVE SMC-PAD Pocket](../midi-controllers/mvave-smc-pad-pocket/README.md) | Retained vendor manual | USB, BLE, wireless hardware route, mappings, and backups | Record wireless adapter, destination, and endpoint results |
| [M-VAVE FM-1](../midi-controllers/mvave-fm-1/README.md) | Retained vendor manual | audio, USB/BLE MIDI, MIDI-IN, learning, mappings, and backups | Record audio level, MIDI endpoint, and patch state |
| [TEYUN A8](../audio-interfaces/teyun-a8/README.md) | Third-party A8 and A4/A6/A8 series sources only; no manufacturer manual found | provisional capability record, safe signal flow, and integration recipes | Photograph labels, retain printed material, and test every connector before treating a port or USB behavior as documented |
| [F998](../audio-interfaces/f998/README.md) | FCC filing, retained bundle manual, and explicitly limited third-party cross-check | provisional capability record, routing, safety, and streaming recipes | Record the retail revision and physical port labels before relying on an unverified route |

## Software

### Active Operational Studies

| Software | Source Coverage | Operational Coverage | Remaining Evidence |
| --- | --- | --- | --- |
| [Carla](../audio-software/carla/README.md) | KXStudio application and manual documentation | plugin-host, patchbay, platform, and device integration records | Record installed version, driver, plugin format, and each route |
| [PipeWire](../audio-software/pipewire/README.md) | PipeWire project, MIDI, and session-manager documentation | Linux baseline, client compatibility, and device-routing checklists | Record distribution, WirePlumber, endpoints, links, and latency |
| [LMMS](../audio-software/lmms/README.md) | LMMS user manual, including MIDI setup | documented MIDI capability, platform baseline, and device checklist | Record audio backend, MIDI endpoint, save/reopen, and audible result |
| [EasyEffects](../audio-software/easyeffects/README.md) | Project and official user manual | effect, safety, input/output, preset, and device-binding records | Record PipeWire version, device, chain, preset, and bypass result |
| [Equalizer APO](../audio-software/equalizer-apo/README.md) | Project-owned SourceForge page and configuration reference | Windows device, filter, application, and recovery checklists | Record Windows build, selected endpoint, filters, application mode, and restore result |

### Catalog-Only Software

The entries below have official-source-backed catalog fields but do not yet have
an active study folder. That is an intentional scope boundary, not a claim that
their device integration has been tested.

| Software | Catalog Coverage | To Promote To Active Study |
| --- | --- | --- |
| Ardour | platform, license, function, manual URL | create a study folder and record audio/MIDI backend tests |
| REAPER | platform, license, function, manual URL | create a study folder and record ALSA/JACK or Windows driver tests |
| Bitwig Studio | platform, license, function, manual URL | create a study folder and record its KeyLab DAW script and audio route |
| Qtractor | platform, license, function, manual URL | create a study folder and record ALSA/JACK setup |
| Audacity | platform, license, function, manual URL | create a study folder and record capture/export workflow |
| VCV Rack | platform, license, function, manual URL | create a study folder and record MIDI/audio module route |
| yabridge | platform, license, function, project URL | create a study folder and record Wine, bridge, and plugin compatibility |
| OBS Studio | platform, license, function, knowledge-base URL | create a study folder and record audio-monitoring and streaming routes |

The authoritative URLs, current platform classification, licensing model, and
manual URL for every catalog-only entry are in
[audio-software/catalog.json](../audio-software/catalog.json).

## Evidence Rules

1. Keep original vendor manuals where they can be retained and record checksum,
   source URL, and extraction state.
2. Prefer a manufacturer, project, regulatory filing, or project-owned
   documentation site. Identify third-party material as provisional.
3. Keep routing facts in capability records and recipes. Keep host-specific
   port names, cable details, device labels, and mapping results in observation
   logs.
4. Before changing firmware, presets, mappings, or system audio settings, add a
   recovery record and preserve the previous state.
5. Do not move a catalog-only entry to an active study until at least one local
   baseline is recorded.

Use this file with [device-catalog/schema.md](schema.md) and
[audio-software/README.md](../audio-software/README.md) when adding or
reviewing a study.
