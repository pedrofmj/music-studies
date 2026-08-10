# Pedro Performance Rig

This directory contains the portable authored definition of the first
Performance Rig. Schema version `music-studies/performance-rig/v1` uses JSON
Schema Draft 2020-12.

Current status: schema, stable-slot, and Hardware Preset extraction only. No
installer, service, runtime, or live audio/MIDI tool reads this directory. The
protected `setup.json`, Carla project, services, and graph remain the
production authority until the later parity and cutover gates pass.

`rig.json` records the five stable controller slots from the protected rack.
Each slot orders selectors from model, semantic alias, and endpoint purpose to
an optional local discriminator and optional USB ID. The four M-VAVE devices
share USB ID `4353:4b4d`, so that ID is supporting evidence and cannot
distinguish them by itself. Future platform bindings will resolve these
portable selectors to Linux and Windows device identities.

All five Hardware Preset IDs resolve to files in `hardware-presets`. Arturia,
SMK-25, and SMC-Mixer assignments are verified. SMC-PAD and SMC-PAD Pocket are
explicitly partial because the protected capture proves their routed note
streams but does not contain each pad's exact channel and note number. The Rig
Profile, Device Profile, and trigger IDs remain planned forward references.

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
python3 docs/tools/music-rig/validate-performance-rig.py \
  --validate-root src/performance-rigs/pedro-performance-rig \
  --authority-setup docs/tools/airstar-live-setup/setup.json
```
