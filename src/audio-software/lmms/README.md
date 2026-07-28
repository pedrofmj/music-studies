# LMMS Study And Integration

LMMS is the first active software profile in the audio-software inventory. Its
scope is a reproducible pattern-based MIDI production workflow that can be
documented and tested on both Linux and Windows.

The catalog record is at [LMMS in catalog.json](../catalog.json). The official
project resources are the [download page](https://lmms.io/download) and the
[user manual](https://docs.lmms.io/user-manual/).

## Working Areas

- [Profile](profile.json) records scope, supported platforms, and test status.
- [Learning](learning/README.md) provides the first four hands-on sessions.
- [Reference](reference/README.md) holds platform and device test records.
- [Manual](manual/README.md) tracks official documentation and local retention.

## Current Evidence Status

The official LMMS user manual is recorded as online documentation. No manual
PDF or extracted text is stored yet. Do not treat this as missing
documentation; it means there is no retained PDF evidence in this repository.
When an authorized official PDF becomes available, add it under manual/source,
extract it into manual/extracted, and update manual/manuals.json.

## Cross-Platform Rule

Linux and Windows need separate observed test records. The same project can be
used as a test artifact, but audio backend, MIDI endpoint names, interface
drivers, plugin availability, and export results must be verified per OS.
