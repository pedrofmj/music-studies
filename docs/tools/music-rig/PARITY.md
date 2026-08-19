# Materialized Semantic Parity Contract

`verify-materialized-rig-parity.py` is a read-only authoring gate for a bundle
created by `materialize-compiled-rig.py`. It proves that relocation and
deterministic rendering preserve the protected Carla and Patchbay semantics.
It does not install or activate the bundle.

## Authorities

The verifier reads four protected repository sources:

- `pedro.uproject` as the Carla plugin, plugin-asset, and project-connection
  authority;
- its `project.json` metadata as the 49-plugin and 111-connection count
  authority;
- `pedro-live-rack-patchbay.json` as the deployment-link authority; and
- `setup.json` as the 116-link count and protected source-checksum authority.

Before inventory comparison, it verifies that the materialized root remains a
strict descendant of the system temporary directory, that the manifest retains
its authoring-only safety state, and that its two output paths, byte counts, and
SHA-256 checksums match the files being inspected. It also rechecks the
protected Carla and Patchbay checksums using canonical LF text bytes so the
result is portable across Linux and Windows checkouts.

## Semantic Inventories

### Carla Plugins

Each of the 49 unique plugin names maps to a fingerprint of its complete XML
`Plugin` subtree. This includes type-specific identity, active state, options,
parameters, MIDI mappings, custom data, state chunks, and their ordering.

`Binary` and `Filename` values are normalized to `<home>` and
`<soundfont-root>` tokens before fingerprinting. That permits the intended
relocation while rejecting every other plugin or parameter change.

### Carla Plugin Assets

The 18 nonempty `Binary` and `Filename` records form a multiset keyed by plugin
name, XML field, and normalized path. A removed path, unexpected path, changed
owning plugin, or changed multiplicity therefore produces a direct asset
diagnostic in addition to the full-plugin fingerprint mismatch.

### Carla Connections

The 111 `ExternalPatchbay/Connection` records form a multiset of exact
`Source` and `Target` pairs. Comparing a multiset preserves duplicate
multiplicity while allowing serialization order to change.

### Patchbay Links

The 116 deployment links form a multiset containing:

- link kind;
- each endpoint's direction and port name;
- semantic `alias` and `name_selector` values;
- DSP format; and
- MIDI classification.

Transient numeric node, port, and link IDs and backend object paths are not
semantic identity. The protected reference sink selectors are normalized to
the explicit stereo sink recorded by the materialization manifest; all other
selectors must match exactly.

## Acceptance

Passing requires both the protected and materialized inventories to contain
exactly 49 plugins, 18 plugin asset references, 111 Carla project connections,
and 116 Patchbay links. The plugin map and all three multisets must then match
exactly. Count-only equality is not sufficient.

Run the verifier after temporary materialization:

~~~bash
python3 docs/tools/music-rig/verify-materialized-rig-parity.py \
  --materialized-root "$work_root/bundle"
~~~

A passing invocation prints the four protected counts and
`activation=none`. It writes no report, state, cache, service, project, or graph
data.

The self-test proves the passing inventory and rejects manifest corruption,
incorrect Rig identity, count-preserving plugin-parameter mapping drift, a
missing plugin, a missing plugin asset, a removed Carla connection, a removed
Patchbay link, and count-preserving Patchbay selector drift. These tests
operate only on temporary copies and confirm that the protected sources remain
unchanged.

This gate does not prove that external assets or plugins are installed, that
services are available, or that a live Carla/PipeWire graph matches the bundle.
The portable protected-single-rig regression separately proves that the legacy
materializer reports controlled missing assets and endpoints, that its
read-only output is independent of the working directory, and that every
protected installer, validator, helper, service, project, and graph artifact
retains its baseline checksum. Actual host capability and live-graph evidence
remain later platform-validation milestones.
