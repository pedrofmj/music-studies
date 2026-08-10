# Pedro Performance Rig

This directory contains the portable authored definition of the first
Performance Rig. Schema version `music-studies/performance-rig/v1` uses JSON
Schema Draft 2020-12.

Current status: schema and stable-slot foundation only. No installer, service,
runtime, or live audio/MIDI tool reads this directory. The protected
`setup.json`, Carla project, services, and graph remain the production
authority until the later parity and cutover gates pass.

`rig.json` records the five stable controller slots from the protected rack.
Each slot orders selectors from model, semantic alias, and endpoint purpose to
an optional local discriminator and optional USB ID. The four M-VAVE devices
share USB ID `4353:4b4d`, so that ID is supporting evidence and cannot
distinguish them by itself. Future platform bindings will resolve these
portable selectors to Linux and Windows device identities.

The profile, preset, and trigger IDs in `rig.json` are planned forward
references. This slice validates the catalogue and slot relationships; later
Milestone 1 work will add and cross-check the referenced documents.

The six schemas are:

- `common.schema.json`: shared identifiers, selectors, capabilities, MIDI,
  ownership, readiness, takeover, state, and switch-safety types;
- `rig.schema.json`: the complete Rig catalogue and stable device slots;
- `rig-profile.schema.json`: one global composition;
- `device-profile.schema.json`: one role for one logical device slot;
- `hardware-preset.schema.json`: controller-local raw message assignments; and
- `switch-triggers.schema.json`: persistent management events translated to
  the same future switch operations as the CLI.

Top-level authored definitions contain semantic capabilities. Operating-system
paths, PipeWire/JACK port names, Carla parameter indices, Windows device IDs,
and service identifiers belong in platform bindings, which are not part of
this schema slice.

Run the offline schema suite from the repository root after installing the
authoring-only dependency:

```bash
python3 -m pip install -r docs/tools/music-rig/requirements-schema.txt
python3 docs/tools/music-rig/validate-performance-rig.py --self-test
```
