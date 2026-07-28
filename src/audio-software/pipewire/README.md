# PipeWire Study And Integration

PipeWire is the Linux-native routing layer for the software inventory. Its
study scope is low-latency audio and MIDI graph behavior, device routing, and
client compatibility for applications such as LMMS and Carla.

The catalog record is at [PipeWire in catalog.json](../catalog.json). The
official sources are the [PipeWire project page](https://pipewire.org/) and
[PipeWire documentation](https://docs.pipewire.org/).

## Platform Boundary

PipeWire is documented here as Linux-native. The official project describes it
as a Linux multimedia project and the catalog records Windows as not offered.
This directory therefore contains Linux observations only. It is not evidence
for a Windows audio-routing configuration.

## Working Areas

- [Profile](profile.json) records the Linux-only scope and test status.
- [Learning](learning/README.md) provides four routing and compatibility sessions.
- [Reference](reference/README.md) holds capability, client, and device evidence.
- [Manual](manual/README.md) tracks official web documentation and local retention.

No manual PDF or extracted text is stored yet. When an authorized official PDF
becomes available, add it under manual/source, extract it into manual/extracted,
and update manual/manuals.json.
