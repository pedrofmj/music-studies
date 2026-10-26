# Arturia SF2 Pack

`package_arturia_sf2_pack.py` inventories the shared Windows profile's active
SF2 assets from a build-time Galaxy Sound Bank root, then stages them into one
portable, content-addressed pack. The source root can be an Insync-synchronized
library during development; its absolute path is never written into pack output.

The current profile catalog resolves to 22 named Carla patch instances across
15 catalog-relative SF2 paths. Identical file contents are stored only once.
This is one pack, not a copy of the complete Galaxy Sound Bank and not one folder
per genre.

The shared routing JSON stays common across platforms. The Windows pack uses the
base patch catalog; `benchmarks/arturia-sf2-assets.android.json` supplies
Android-only asset and preset-label overrides without changing genre IDs, active
layers, pads, or controller mappings.

## Inventory

Run an inventory before legal review:

```bash
python3 docs/tools/music-rig/packaging/package_arturia_sf2_pack.py \
  --source-root "/path/to/Pack de Timbres/Library" \
  --inventory-output /tmp/arturia-sf2-inventory.json
```

The inventory lists each active catalog-relative path, byte size, SHA-256, and
logical patch ID. It does not copy assets. Use it to identify and review the exact
files intended for distribution.

## Redistribution Gate

Pack creation requires a separate license manifest with schema
`music-studies/arturia-sf2-licenses/v1`. It must contain exactly one record for
each path in the inventory. Each record must provide:

- `approved_for_distribution: true`, set only after redistribution rights are confirmed.
- `sha256`, matching the exact inventory hash.
- `license_id` and the complete `license_text` to include in the pack.
- `source_url` or a written `source_reference`, plus required `attribution`.
- `reviewed_by` and `reviewed_on` for the approval record.

Missing approval, a hash mismatch, or incomplete notice metadata fails closed.
The tool checks that an approval is explicit and bound to the exact file bytes;
it does not determine whether that legal approval is correct. Existing external
asset records in this repository are not redistribution grants. A user-provided
rights attestation may use `source_reference` when no public license URL exists;
the pack notice records it as a user attestation, not as the original upstream
license.

## Stage A Pack

After the exact files have approved records in a JSON license manifest:

```bash
python3 docs/tools/music-rig/packaging/package_arturia_sf2_pack.py \
  --source-root "/path/to/Pack de Timbres/Library" \
  --license-manifest /path/to/approved-arturia-sf2-licenses.json \
  --output dist/arturia-sf2-pack
```

The output contains `manifest.json`, an Android-friendly `asset-index.tsv`, one
`assets/<sha256>.sf2` file per distinct content hash, and the corresponding
notice files under `licenses/`. The pack manifest maps each patch ID and
source-relative catalog path to its packaged asset. The output directory must
not already exist; an exclusive sibling lock serializes pack builds, and atomic
no-replace publication protects a destination created concurrently. File and
directory timestamps and permissions are normalized so repeated builds from the
same inputs produce the same pack contents and metadata.

For the Android variant, include both override arguments:

```bash
python3 docs/tools/music-rig/packaging/package_arturia_sf2_pack.py \
  --source-root "/path/to/Pack de Timbres/Library" \
  --platform-overrides docs/tools/music-rig/benchmarks/arturia-sf2-assets.android.json \
  --license-manifest /path/to/approved-android-sf2-licenses.json \
  --output dist/arturia-sf2-pack-android
```

The current override uses `CFX Premium.sf2` and `JF Soft Grand Piano.sf2` from the
Galaxy library, plus the existing Motif SF2 for its exact preset label. The variant
includes a sanitized `android-catalog-overrides.json` containing only patch IDs,
pack-relative logical paths, and preset labels. Current review packs are staged
outside Git; their notices record the user's rights attestation, not original
upstream licenses. Google Play delivery size has not yet been verified.

## Integration Boundary

After staging, `materialize_arturia_sf2_pack_project.py` can make a new Carla
project copy whose SF2 `Filename` and `Label` fields point at the installed pack:

```bash
python3 docs/tools/music-rig/packaging/materialize_arturia_sf2_pack_project.py \
  --template-project /path/to/template.carxp \
  --pack-root dist/arturia-sf2-pack \
  --output dist/runtime-project.carxp
```

The materializer verifies every packed hash and license notice, requires exactly
one SF2 plugin for each pack patch ID, and changes only those plugins' filename
and label fields. It preserves the caller-provided project and its existing audio
patchbay connections. The generated project stores absolute paths to the pack, so
materialize it after installation or repeat materialization if the pack moves.

The Android host can consume an Android variant's asset index and lazily extract
selected files to app-private cache storage. Windows installer integration and
Android store delivery are still pending. Pack artifacts are not tracked in Git;
the user-attested review packs should not be treated as independent upstream
license verification.

The proposed cloud roots are
`https://echora.egypteam.com/downloads/music-rig/sf2/v1/windows/` and
`https://echora.egypteam.com/downloads/music-rig/sf2/v1/android/`, with each
variant's `assets/` directory uploaded below its root. No upload has been made;
the matching server route must be configured and confirmed first.
