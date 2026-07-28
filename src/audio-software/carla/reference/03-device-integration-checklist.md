# Device Integration Checklist

Test plugin loading, MIDI, and audio independently before testing a complete
patchbay workflow. One successful controller mapping does not prove that the
audio interface is selected, and one successful rack does not prove that all
plugin formats work.

| OS | Device | Role | Connection | Endpoint or driver | Plugin or rack | Test action | Result | Date |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Linux | | MIDI controller | | | | Trigger a test note or parameter | not tested | |
| Linux | | Audio interface | | | | Monitor a rack chain | not tested | |
| Windows | | MIDI controller | | | | Trigger a test note or parameter | not tested | |
| Windows | | Audio interface | | | | Monitor a rack chain | not tested | |

For a passed entry, add the Carla version, exact endpoint or driver name,
plugin format, patchbay connection, and required settings in a dated note below
this table.
