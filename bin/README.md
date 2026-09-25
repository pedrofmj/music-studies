# PipeWire Patchbay Export

`pipewire-patchbay-json` exports the current PipeWire ports and links to a
JSON document, then recreates those links later. It does not create hardware
devices, plugin instances, or applications; start those first.

## Independent Device Modes

`music-rig-device-mode` selects a device mode without changing another device.
The currently supported non-Arturia modes are the existing runtime modes:

```bash
music-rig-device-mode --device smk25-main --mode ambient-pad-layers
music-rig-device-mode --device smc-mixer-main --mode eight-band-eq
music-rig-device-mode --device smc-pad-main --mode drum-set
music-rig-device-mode --device smc-pad-pocket --mode drum-set
```

Arturia requests use the existing selector FIFO:

```bash
music-rig-device-mode --device arturia-main --mode gospel-keys
```

The command restores and verifies the independent MIDI routes. It does not
start or stop Carla and does not change another device's active mode. Additional
device modes can be added later without changing the Arturia selector.

## Runtime Profiles

`music-rig-profile` is the single entry point for versioned runtime profiles,
rollback, events, and reset scopes. Install profiles under
`~/.config/music-rig/profiles` before using it from Echora:

```bash
music-rig-profile list
music-rig-profile status
music-rig-profile apply validated-4-5
music-rig-profile rollback
music-rig-profile reset smk25
```

`music-rig-setup` is the package-level setup boundary used by Echora's
Performance Rig Setup dialog:

```bash
music-rig-setup status
music-rig-setup validate
music-rig-setup install
music-rig-setup reinstall
music-rig-setup uninstall
music-rig-setup repair
```

The package payload is staged under
`~/.local/share/echora/performance-rig-package`. Large user assets remain
external and are validated before install/reinstall succeeds.

## Run on airstar

Copy the script to the host and make it executable:

```bash
scp bin/pipewire-patchbay-json pedro.ferreira@airstar:~/bin/
ssh pedro.ferreira@airstar 'chmod +x ~/bin/pipewire-patchbay-json'
```

## Daily shortcuts

After intentionally changing the patchbay, refresh the default snapshot and
restart the automatic restore service with:

```bash
~/bin/pipewire-patchbay-refresh
```

Take a complete snapshot now. By default it writes to
`~/.local/state/pipewire-patchbay/patchbay.json`:

```bash
~/bin/pipewire-patchbay-json --take-snapshot
```

Check and restore the complete connected graph component that contains MIDI
ports. This includes MIDI event routes and downstream instrument, mixer, and
output links, resolving endpoints by name:

```bash
~/bin/pipewire-patchbay-json --check-and-restore
```

Both commands accept one optional JSON file path.

Use `--dry-run` to inspect without invoking a connection restore:

```bash
~/bin/pipewire-patchbay-json --check-and-restore --dry-run
```

## Watch Mode

`--watch-and-restore` is a foreground daemon loop. It creates the snapshot if
it is missing, then polls the MIDI-connected component every two seconds and
restores missing resolvable links.

```bash
~/bin/pipewire-patchbay-json --watch-and-restore
```

Set a whole-second poll interval or use another snapshot path:

```bash
~/bin/pipewire-patchbay-json --watch-and-restore --interval 5 PATCHBAY.json
```

To run it as a managed user service:

```bash
systemd-run --user --unit=pipewire-patchbay-watch --collect \
  ~/bin/pipewire-patchbay-json --watch-and-restore
```

## Event Watch Mode

`--watch-events-and-restore` listens to PipeWire registry events instead of
polling. It waits for a two-second quiet period after an event burst, then
checks and restores the MIDI-connected component once.

```bash
~/bin/pipewire-patchbay-json --watch-events-and-restore --settle 2
```

The installed `pipewire-patchbay-watch.service` uses this event-driven mode.

Export the complete Patchbay:

```bash
ssh pedro.ferreira@airstar '~/bin/pipewire-patchbay-json export' \
  > airstar-patchbay.json
```

Export only MIDI endpoints and their routes:

```bash
ssh pedro.ferreira@airstar \
  '~/bin/pipewire-patchbay-json export --midi-only' \
  > airstar-midi-patchbay.json
```

Restore an exported graph after all its devices and applications have started:

```bash
scp airstar-patchbay.json pedro.ferreira@airstar:/tmp/
ssh pedro.ferreira@airstar \
  '~/bin/pipewire-patchbay-json import /tmp/airstar-patchbay.json'
```

PipeWire global port IDs change when the daemon, applications, or devices are
restarted. The default import therefore restores a graph exactly only while
the saved IDs still exist. For recovery after a restart, use `--names`; it
first resolves each endpoint by its PipeWire `port.alias`, then falls back to
the exact `node.name:port.name` selector:

```bash
ssh pedro.ferreira@airstar \
  '~/bin/pipewire-patchbay-json import --names /tmp/airstar-patchbay.json'
```

An import is additive and idempotent: existing saved links are skipped and
unrelated links remain. Use `--midi-component` with `import` to select the
complete connected graph component that contains MIDI ports. Name-based import
stops with a non-zero status when a saved endpoint is missing or its selector
has become ambiguous. The final
summary reports every restored, already-present, unresolved, and failed link.

## JSON shape

The JSON document has `nodes`, `ports`, and `links`. Each link embeds its
output and input endpoint. `id` is the PipeWire global port ID used by the
default importer. `--names` uses `alias` when it is unique for the saved
direction, falling back to `name_selector` otherwise.

`midi` is calculated from PipeWire's `format.dsp == "8 bit raw midi"`. The
default `all` scope additionally records non-MIDI endpoints and audio/video
links, which makes it a complete Patchbay snapshot. `midi-only` documents only
MIDI ports and MIDI links.

PipeWire records graph connections, not plugin-internal parameter values. On
airstar, Carla owns plugin parameters and stores them in its `.uproject`; load
the intended Carla project before restoring graph links.
