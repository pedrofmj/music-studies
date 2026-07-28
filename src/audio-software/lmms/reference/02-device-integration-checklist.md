# Device Integration Checklist

Test audio and MIDI independently before treating a complete workflow as
working. One successful controller test does not establish audio-interface
driver support, and one successful audio test does not establish controller
mapping.

| OS | Device | Role | Connection | Endpoint or driver | Test action | Result | Date |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Linux | | MIDI controller | | | Trigger a test note | not tested | |
| Linux | | Audio interface | | | Monitor a test pattern | not tested | |
| Windows | | MIDI controller | | | Trigger a test note | not tested | |
| Windows | | Audio interface | | | Monitor a test pattern | not tested | |

For a passed entry, add the LMMS version, exact endpoint or driver name, and
any required setting in a dated note below this table.
