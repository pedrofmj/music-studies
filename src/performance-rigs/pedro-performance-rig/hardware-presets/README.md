# Current Hardware Presets

These portable documents extract controller-local assignments from the
protected current rack. They are authoring input only and are not loaded by a
service or connected to MIDI.

| Preset | Evidence |
| --- | --- |
| `arturia-current-rack` | Verified: 9 faders, 9 knobs, relative encoder, and click |
| `smk25-current-pad-layers` | Verified: 8 knobs, 8 Side-A toggles, Stop, and Play |
| `smc-mixer-current-cc` | Verified: 8 faders |
| `smc-pad-current-notes` | Partial: routed notes plus measured Knob 1 and Knob 2 |
| `smc-pad-pocket-current-notes` | Partial: routed notes |

Verified presets contain only exact MIDI type, channel, and number
assignments. Partial pad presets use an explicit wildcard note stream and list
what remains unknown. The validator rejects a wildcard in a preset marked
`verified`.

The protected
[`setup.json`](../../../../docs/tools/airstar-live-setup/setup.json) and
[`controller-profiles.md`](../../../../docs/tools/airstar-live-setup/controller-profiles.md)
remain authoritative. Promoting either pad preset requires a raw capture of
every relevant pad's endpoint, channel, message type, note number, press and
release values, preset, and bank.
