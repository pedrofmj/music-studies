# Performance Rig Compiler

`compile-performance-rig.py` is the authoring-only deterministic compiler for
the Configurable Performance Rig. This first Milestone 2 slice emits the
validated compilation envelope, selected composition, requirements, source
manifest, and fingerprints. Direct MIDI lookup tables, ownership tables, and
graph deltas are deliberately deferred to the next compiler slice.

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
nine-case compiler suite. The suite verifies exact golden bytes, repeated
compilation, different working directories, canonical fingerprint
recalculation, no-write behavior, source-overwrite rejection, deterministic
missing-reference and missing-capability diagnostics, and independent portable
and binding fingerprints. The same golden fixture runs on hosted Linux and
Windows.

The checked-in
[`compiled-current-envelope.json`](tests/fixtures/compiled-current-envelope.json)
is test evidence only. No runtime or installer reads it.
