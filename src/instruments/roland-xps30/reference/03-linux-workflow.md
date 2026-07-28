# Linux Workflow

The goal is to keep routine XPS-30 maintenance possible from Ubuntu: downloads,
archive inspection, USB file management, checksums, sample organization, and
backup storage.

Roland official JUNO-DS Tone Manager and Librarian downloads are listed for
Windows and macOS, not Linux. Treat those tools as optional exception paths.
Prefer on-keyboard editing plus USB file workflows unless a task truly requires
the official desktop software.

## Principles

- Format the USB drive on the XPS-30 when Roland official procedure requires it.
- Keep downloaded Roland archives unchanged in a `downloads/` area outside the
  keyboard working USB folder.
- Extract archives into dated working directories.
- Read the included Roland readme before copying files to USB.
- Keep a backup before system updates, expansion changes, and major favorite or
  performance edits.
- Record checksums for downloaded archives.

## Suggested Local Layout

```text
src/instruments/roland-xps30/
  backups/
  expansions/
  learning/
  reference/
  samples/
  references/
```

If large audio files or binary backups become too heavy for normal Git history,
move them to Git LFS or a separate archived storage path and keep manifests in
this repository.

## Useful Ubuntu Commands

Identify the USB drive:

```bash
lsblk -o NAME,LABEL,FSTYPE,SIZE,MOUNTPOINTS
```

Record a checksum:

```bash
sha256sum xps30_sys_v212.zip
```

Inspect an archive:

```bash
unzip -l xps30_sys_v212.zip
```

Copy a prepared folder to a mounted USB path:

```bash
rsync -av --progress prepared-usb/ /media/$USER/XPS30/
```

Before running copy commands, confirm the mounted path and contents manually.

## Hardware-Dependent Details

The following details must come from the installed XPS-30 system version and
its official manual, because menu labels and USB folder layouts can vary by
procedure: USB folder names, backup/restore menu path, expansion installation
path, sample-import path, and FAT32 media behavior. Prefer media formatted by
the keyboard whenever Roland official procedure calls for it.
