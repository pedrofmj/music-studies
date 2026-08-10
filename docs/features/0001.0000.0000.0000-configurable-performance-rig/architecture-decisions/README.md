# Configurable Performance Rig Architecture Decisions

This directory contains short, implementation-binding decisions for feature
`0001.0000.0000.0000`.

- [ADR 0001: Defer Native PipeWire Graph Control](0001-pipewire-graph-control-deferral.md)
  is accepted. Native PipeWire graph mutation starts no earlier than Milestone
  6.
- [ADR 0002: Carla Control And Prepared-Engine Boundary](0002-carla-control-and-prepared-engine-boundary.md)
  is accepted. Early switches reuse loaded engines through certified controls;
  lifecycle, state-load, project, and patchbay operations require preparation.
- [ADR 0003: Performance Acceptance Thresholds](0003-performance-acceptance-thresholds.md)
  is accepted. It fixes the portable timing, daemon-resource, and zero-failure
  stability gates and requires a separate no-event idle observation.
- [ADR 0004: Windows Local IPC Uses Named Pipes](0004-windows-local-ipc-named-pipes.md)
  is accepted. Windows uses local message-mode named pipes with explicit local
  access, deadline, and cleanup requirements.
- [ADR 0005: JSON-C For Control-Plane Parsing](0005-json-c-control-plane-parsing.md)
  is accepted. Linux and Windows use `json-c` only while preparing immutable
  runtime generations; real-time callbacks never parse JSON.

Accepted records constrain later schemas, compiler output, runtime behavior,
tests, and deployment. A decision may be superseded only by another recorded
decision that identifies the replacement and migration impact.
