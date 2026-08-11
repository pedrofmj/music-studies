# Pedro Performance Rig

This directory contains the portable authored definition of the first
Performance Rig. Schema version `music-studies/performance-rig/v1` uses JSON
Schema Draft 2020-12.

Current status: schema, stable-slot, Hardware Preset, Device Profile, initial
Rig Profile, Linux Platform Binding extraction, and deterministic compilation
envelope. Only authoring validation and compilation tools read this directory;
no installer, service, runtime, or live audio/MIDI tool consumes it. The
protected `setup.json`, Carla project, services, and graph remain the production
authority until the later parity and cutover gates pass.

`rig.json` records the five stable controller slots from the protected rack.
Each slot orders selectors from model, semantic alias, and endpoint purpose to
an optional local discriminator and optional USB ID. The four M-VAVE devices
share USB ID `4353:4b4d`, so that ID is supporting evidence and cannot
distinguish them by itself. Platform bindings resolve these portable selectors
to operating-system device identities.

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
exclusive or read-only.

[`rig-profiles/full-live-rack.json`](rig-profiles/full-live-rack.json) composes
all five roles as the default and fallback Rig Profile. It declares the
aggregate endpoint and musical capabilities, pins the eighteen current sound
engines, identifies the shared drum engine and effects, and retains the safe
takeover and rollback policies. It is authored data only; the protected setup
remains the active default. The resolved
[`switch-triggers.json`](switch-triggers.json) management catalogue is empty,
so it enables no MIDI-triggered switching.

The seven schemas are:

- `common.schema.json`: shared identifiers, selectors, capabilities, MIDI,
  ownership, readiness, takeover, state, and switch-safety types;
- `rig.schema.json`: the complete Rig catalogue and stable device slots;
- `rig-profile.schema.json`: one global composition;
- `device-profile.schema.json`: one role for one logical device slot;
- `hardware-preset.schema.json`: controller-local raw message assignments;
- `platform-binding.schema.json`: backend device identities, semantic target
  locators, machine paths, lifecycle resources, and evidence status; and
- `switch-triggers.schema.json`: persistent management events translated to
  the same future switch operations as the CLI.

Top-level authored definitions contain semantic capabilities. Operating-system
paths, PipeWire/JACK port names, Carla parameter indices, Windows device IDs,
and service identifiers belong only in Platform Bindings. The authored
[`airstar-current`](platform-bindings/linux/airstar-current.json) Linux binding
resolves the complete `full-live-rack` contract against the protected setup.
It is authoring-only and cannot mutate or activate the runtime. The Windows
document under the validator fixtures proves the portable contract shape; it
is explicitly not physical Windows support or certification.

Root validation checks the Rig, all Hardware Presets, Device Profiles, Rig
Profiles, Platform Bindings, and Switch Triggers. It resolves slot, model,
endpoint, preset, control, profile, capability, resource, binding, trigger
source, and trigger-operation references; checks required-slot coverage,
aggregate capabilities and readiness, pinned and shared resources,
initial-state ownership, explicit composition ownership conflicts, management
MIDI assignment, readiness ceilings, and consumed-event mapping conflicts; and
locks the current Linux binding to protected Airstar aliases, paths, checksums,
and services. It reads authored files and protected evidence only; it does not
connect to the live rig.

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

python3 docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --check-only
```

The compiler emits only to an explicit output path and refuses to overwrite
authored source. Its Milestone 2 lookup, ownership, graph-delta, and fingerprint
contracts are documented in
[`COMPILER.md`](../../../docs/tools/music-rig/COMPILER.md). It does not
materialize or activate Carla, Patchbay, MIDI, audio, service, or runtime state.
