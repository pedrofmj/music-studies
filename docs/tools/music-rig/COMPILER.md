# Performance Rig Compiler

`compile-performance-rig.py` is the authoring-only deterministic compiler for
the Configurable Performance Rig. It emits the validated compilation envelope,
selected composition, direct MIDI lookup data, selected platform targets,
consolidated ownership, graph delta, source manifest, and fingerprints.

The compiler does not install a service, connect to MIDI or audio, write Carla
or Patchbay state, or activate its output. It has no default output path.

## Inputs

Compilation requires:

- one validated authored Rig root;
- one explicit Platform Binding ID; and
- an optional Rig Profile ID, defaulting to `default_rig_profile`.

The compiler invokes the same pinned schema and semantic validator used by
CTest before resolving the selection. It rejects missing or ambiguous bindings,
unresolved profiles, incomplete binding capabilities, and every existing
catalogue validation failure before producing output.

## Canonical Output

The output uses schema `music-studies/compiled-performance-rig/v1`. JSON is
encoded as UTF-8 with ASCII escapes, keys sorted recursively, two-space
indentation, and one final newline. Arrays that represent sets or catalogues
are sorted before serialization. Source paths are relative to the Rig root and
always use `/`; absolute working-directory and output paths are excluded.

The envelope records:

- the selected Rig, Rig Profile, Device Profiles, Hardware Presets, readiness,
  capabilities, pinned capabilities, and resources;
- the selected Platform Binding ID, platform, and evidence status;
- stable-slot input bindings, ordered mapping rows, and their direct dispatch
  index;
- selected semantic target bindings and consolidated per-target ownership;
- the calculated graph delta and control-only eligibility;
- individual canonical fingerprints for every schema and portable source;
- independent aggregate schema-set, portable-source, and Platform Binding
  fingerprints; and
- one `definition_fingerprint` for the generated definition.

All fingerprints use lowercase SHA-256 and the `sha256:` prefix. A document's
canonical fingerprint hashes its compact JSON form with sorted keys and no
insignificant whitespace. Aggregate fingerprints hash the canonical ordered
manifest. The `definition_fingerprint` hashes the complete compiled document
with only the `definition_fingerprint` field itself omitted.

This definition makes source fingerprints insensitive to source formatting,
working directory, output location, and operating-system path syntax. Selecting
a different binding leaves the portable-source fingerprint unchanged while
changing the binding and definition fingerprints.

## Runtime Tables

`input_bindings` resolves each selected stable device slot to the adapter,
identity, status, and required endpoint locators from the Platform Binding.

`mappings` contains the resolved Device Profile actions. Every row includes the
Hardware Preset control and raw MIDI event, semantic target, transform, and
resolved takeover mode where takeover applies. Relative source and transform
encodings must agree. `mapping_index` maps the direct dispatch key
`slot|message-type|channel|number` to exactly one row position; the event edge
remains in the row for the runtime edge check. Mapping rows do not contain
operating-system identifiers and remain unchanged when only the Platform
Binding changes.

`target_bindings` contains only the selected composition's non-input
capabilities, pinned capabilities, and resources. Each key resolves directly to
the selected adapter, availability status, and locator.

`ownership` uses `kind|semantic-target` keys. Compatible
`shared-event-destination` and `read-only` claims are consolidated with a
deterministically ordered owner list. Mixing ownership modes for one key fails
compilation.

## Graph Delta

The graph delta compares selected engine, effect, and route requirements with
targets already marked `available` by the selected Platform Binding. It records
created and removed links, created and removed objects, and metadata changes as
separate deterministic operation lists and counts. Extra existing resources are
preserved, so compilation never infers removals merely because a profile does
not require them.

The current `full-live-rack` delta is empty in all five categories and is
classified as control-only eligible. Any control-only compilation with a
nonempty delta is rejected. The format reserves nonempty operations for later
prepared or cold staged execution, but no such executor exists in this slice;
the compiler never applies a delta, and both `graph_delta.applied` and
`safety.applies_graph_delta` remain `false`.

## Commands

Validate and compile entirely in memory:

~~~bash
python3 docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --check-only
~~~

Write canonical JSON to an existing explicit output directory:

~~~bash
python3 docs/tools/music-rig/compile-performance-rig.py \
  --rig-root src/performance-rigs/pedro-performance-rig \
  --platform-binding airstar-current \
  --output /tmp/pedro-performance-rig.compiled.json
~~~

`--check-only` and `--output` are mutually exclusive and one is required. The
compiler refuses to overwrite any authored source, schema, or Platform Binding.
Output replacement is atomic within the destination directory. Check-only mode
writes no output or Python bytecode.

## Verification

The default CTest suite checks the current catalogue in memory and runs the
thirteen-case compiler suite. The suite verifies exact golden bytes, repeated
compilation, different working directories, canonical fingerprint
recalculation, all current table counts and representative mappings,
consolidated ownership, empty and nonempty graph classification, control-only
delta rejection, relative-encoding consistency, no-write behavior,
source-overwrite rejection, deterministic missing-reference and
missing-capability diagnostics, and independent portable and binding output.
The same golden fixture runs on hosted Linux and Windows.

The checked-in
[`compiled-current-envelope.json`](tests/fixtures/compiled-current-envelope.json)
is test evidence only. No runtime or installer reads it.
