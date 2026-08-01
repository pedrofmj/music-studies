# Equalizer APO Study And Integration

Equalizer APO is the Windows system-effects profile in the audio-software
inventory. Its study scope is playback-device attachment, parametric filtering,
controlled monitoring, and repeatable per-device configuration evidence.

The catalog record is at [Equalizer APO in catalog.json](../catalog.json). The
project-owned sources are the [SourceForge project page](https://sourceforge.net/projects/equalizerapo/)
and [configuration reference](https://sourceforge.net/p/equalizerapo/wiki/Configuration%20reference/).

## Platform Boundary

Equalizer APO is documented here as Windows-native. The official project
describes it as an audio equalizer for Windows users, and the catalog records
Linux as not offered. This directory therefore contains Windows observations
only and does not define a Linux system-effects workflow.

## Working Areas

- [Profile](profile.json) records the Windows-only scope and test status.
- [Learning](learning/README.md) provides device, filter, and recovery sessions.
- [Reference](reference/README.md) holds capability, device, and filter evidence.
- [Manual](manual/README.md) tracks official web documentation and local retention.

No manual PDF or extracted text is stored yet. When an authorized official PDF
becomes available, add it under manual/source, extract it into manual/extracted,
and update manual/manuals.json.
