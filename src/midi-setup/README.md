# Airstar MIDI Setup

This directory records the active Linux workstation setup on airstar: the
machine to which the MIDI controllers are connected and that hosts the Carla
live rack.

The initial verification was performed on 2026-08-02 against the live system,
not inferred from a package list. It found PipeWire and WirePlumber running,
the Carla Flatpak process open, and the saved project at
/c/music/carla/pedro.uproject.

## Documents

- [Current State](current-state.md) records the verified graph, controller
  ports, host versions, and system boundaries.
- [Plugin Inventory](plugin-inventory.md) distinguishes package-owned plugins
  from manually copied ones and records the remaining deliberate gaps.
- [Operating Procedure](operating-procedure.md) describes safe startup,
  refresh, expansion, and recovery steps.

## Authoritative Rack

/c/music/carla/pedro.uproject is the authoritative definition of the live
rack. It already contains the MIDI source, the CC transformation, instruments,
mixer, and output connections. Do not create an equivalent permanent graph in
qpwgraph while this project is restoring the same links; doing so makes the
source of truth ambiguous and can duplicate connections.

qpwgraph is installed as a separate inspection and recovery tool. It is useful
when a device name changes or Carla does not restore a connection, but it is
not the primary store for this rack.
