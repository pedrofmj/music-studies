# Carla Study And Integration

Carla is the active plugin-host and patchbay profile. Its scope is to build
reproducible rack chains and audio/MIDI routing workflows that can be tested on
both Linux and Windows.

The catalog record is at [Carla in catalog.json](../catalog.json). The primary
official source is the [KXStudio Carla page](https://kx.studio/Applications:Carla).
The source repository is [falkTX/Carla](https://github.com/falkTX/Carla).

## Working Areas

- [Profile](profile.json) records scope, supported platforms, and test status.
- [Learning](learning/README.md) provides four hands-on host and patchbay sessions.
- [Reference](reference/README.md) holds platform, format, and device evidence.
- [Manual](manual/README.md) tracks official documentation and local retention.
- [Projects](projects/README.md) contains deployable, versioned Carla racks and
  their runtime dependency metadata.

## Current Evidence Status

The KXStudio application page is recorded as official web documentation. No
manual PDF or extracted text is stored yet. When an authorized official PDF is
available, add it under manual/source, extract it into manual/extracted, and
update manual/manuals.json.

## Cross-Platform Rule

Plugin scan results, supported formats, device names, audio drivers, and routing
behavior are all observed per OS. A working Linux rack does not establish an
equivalent Windows workflow, and a successful plugin scan does not establish
reliable real-time audio.
