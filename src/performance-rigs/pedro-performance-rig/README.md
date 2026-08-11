# Pedro Performance Rig

This directory contains the portable authored definition of the first
Performance Rig. Schema version `music-studies/performance-rig/v1` uses JSON
Schema Draft 2020-12.

Current status: schema, stable-slot, Hardware Preset, and current Device
Profile extraction. No installer, service, runtime, or live audio/MIDI tool
reads this directory. The protected `setup.json`, Carla project, services, and
graph remain the production authority until the later parity and cutover gates
pass.

`rig.json` records the five stable controller slots from the protected rack.
Each slot orders selectors from model, semantic alias, and endpoint purpose to
an optional local discriminator and optional USB ID. The four M-VAVE devices
share USB ID `4353:4b4d`, so that ID is supporting evidence and cannot
distinguish them by itself. Future platform bindings will resolve these
portable selectors to Linux and Windows device identities.

All five Hardware Preset IDs resolve to verified files in `hardware-presets`.
The SMC-PAD and SMC-PAD Pocket assignments are checked against the
[2026-08-11 live capture](../../../docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json),
including exact per-pad channel/note assignments and the Pocket's eight
hardware-internal control pads.

The five current Device Profiles resolve under `device-profiles/<slot>/<id>`:

- Arturia `multi-instrument-rack`;
- SMK-25 `ambient-pad-layers`;
- SMC-Mixer `eight-band-eq`;
- SMC-PAD `drum-set`; and
- SMC-PAD Pocket `drum-set`.

They link semantic mappings to controls from the selected Hardware Preset and
declare capabilities, ownership, dependencies, readiness, state, takeover, and
switch safety without platform paths or backend identifiers. The two pad
profiles intentionally share `drum-set.notes`; other current ownership remains
exclusive or read-only. The Rig Profile and trigger IDs remain planned forward
references.

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

Root validation checks the Rig, all Hardware Presets, all Device Profiles,
slot/model/endpoint/preset/control references, and ownership conflicts in the
current one-profile-per-slot composition. It reads authored files and protected
evidence only; it does not connect to the live rig.

Run the offline schema suite from the repository root after installing the
authoring-only dependency:

```bash
python3 -m pip install -r docs/tools/music-rig/requirements-schema.txt
python3 docs/tools/music-rig/validate-performance-rig.py --self-test
python3 docs/tools/music-rig/validate-performance-rig.py \
  --validate-root src/performance-rigs/pedro-performance-rig \
  --authority-setup docs/tools/airstar-live-setup/setup.json \
  --authority-pad-capture \
    docs/tools/music-rig/benchmarks/hardware-preset-airstar-2026-08-11.json
```
