# Pedro Live Rack

This directory is the Git snapshot of Pedro's active Carla performance rack.

## Contents

- `pedro.uproject` is the deployable Carla project copied byte-for-byte from
  `/c/music/carla/pedro.uproject` on `airstar`.
- `project.json` records the capture source, checksum, expected structure, and
  runtime dependencies.
- [Rack configurator](../../../../../docs/tools/carla-live-rack/README.md)
  documents and rebuilds the controller mappings, instruments, and routing.
- [Carla launcher](../../../../../bin/carla-pedro-project) opens the deployed
  project with the required Flatpak working directory.

## Asset Policy

SoundFonts, DecentSampler libraries, and plugin binaries are not stored in
Git. The `.uproject` retains their absolute paths. Every target computer must
provide the same library layout documented in `project.json` and in the rack
configurator.

## Validate

From the repository root:

```bash
python3 docs/tools/carla-live-rack/configure-live-rack.py --check-only \
  src/audio-software/carla/projects/pedro-live-rack/pedro.uproject
sha256sum src/audio-software/carla/projects/pedro-live-rack/pedro.uproject
```

## Deploy To Airstar

Close Carla before replacing the deployed project:

```bash
scp src/audio-software/carla/projects/pedro-live-rack/pedro.uproject \
  pedro.ferreira@airstar:/c/music/carla/pedro.uproject
ssh pedro.ferreira@airstar '~/bin/carla-pedro-project'
```

The PipeWire restore service owns connections outside Carla, including the
final EQ-to-volume-gate and gate-to-system links. It is intentionally separate
from this project snapshot.

## Capture From Airstar

Close or save Carla first, then copy the deployed project back into this
directory and update the checksum and capture time in `project.json`:

```bash
scp pedro.ferreira@airstar:/c/music/carla/pedro.uproject \
  src/audio-software/carla/projects/pedro-live-rack/pedro.uproject
```
