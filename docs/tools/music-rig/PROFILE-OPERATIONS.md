# Profile Operations

`bin/music-rig-profile` is the single operational entry point for selecting a
Music Rig runtime profile and resetting runtime components. Echora's Performance
Rig Operations tab invokes this command rather than editing systemd or PipeWire
state directly.

## Profiles

Profiles are versioned `.env` files in `runtime-profiles/`. Install them on the
performance host under `~/.config/music-rig/profiles/`.

The currently validated simultaneous genre profile is `validated-4-5`. Genres
6-10 are validated individually and should not be combined without a separate
resource test.

```bash
music-rig-profile list
music-rig-profile status
music-rig-profile apply validated-4-5
music-rig-profile rollback
music-rig-profile repair-routes
music-rig-profile repair-audio
music-rig-profile recover
music-rig-profile events --limit 30
```

`apply` saves the previous environment, restarts the selector, waits for the
expected warm-genre event, and restores the previous environment if readiness
fails.

`repair-routes` is additive and idempotent. It restores the Arturia
keyboard-to-router link, the router-to-master-control link, MIDI device routes,
and missing internal connections from the protected Carla patchbay. It resolves
controllers by their descriptive PipeWire port aliases, not transient ALSA
client numbers, PipeWire node IDs, or USB hub port numbers. In particular, the
SINCO controllers are matched by names such as `SMC-PAD Pocket-Master`; their
shared USB product ID is not used to distinguish them. The command waits up to
30 seconds for MIDI source and target ports that may enumerate late. It never
restarts the selector or removes existing links; if its router ports are absent,
it reports the selector state instead.

`repair-audio` restores the performance-rig output from `Arturia Main Volume
Encoder` to the selected Echora audio sink. For the analog stereo sink it also
selects the headphone/P2 port.

`recover` is the post-boot recovery path. It starts only inactive rack services
in dependency order, waits for the live Carla ports and current USB MIDI aliases,
then adds missing audio and MIDI links. It does not reset the rig or restart
healthy services. In the Echora MIDI Router Operations tab, this is exposed as
`Recover Rig`.

## Post-Reboot Recovery

After `landstar` reboots with both hubs and all controller cables already
connected, log in and wait for PipeWire/WirePlumber to start. Open Echora MIDI
Router > Performance Rig > Operations and press `Recover Rig`. It starts the
inactive Carla, encoder, SMK-25, and Arturia selector services, waits for their
ports and the descriptive USB MIDI aliases to appear, then restores routes. No
USB cable replug is part of the recovery path. The command reports any device
alias that remains unavailable instead of claiming full recovery.

Use `Repair Routes` when services and Carla are already active but links are
missing. It resolves the currently enumerated KeyLab `capture_0` port and the
M-VAVE `capture_1` aliases even if ALSA client numbers or PipeWire's `SINCO N`
node labels changed after boot.

The fallback CLI sequence is:

```bash
music-rig-profile recover
# if Arturia remains silent after replugging only the Arturia USB cable:
music-rig-profile repair-routes
```

## Resets

```bash
music-rig-profile reset full
music-rig-profile reset emergency
music-rig-profile reset arturia
music-rig-profile reset smk25
music-rig-profile reset output
music-rig-profile reset genres
```

`full` restarts the protected Carla rack, encoder, and selector. `emergency`
backs up runtime state, stops all Music Rig services, removes orphan genre
Carla processes, clears SMK state, rebuilds services in dependency order, and
returns to engine-only Fast Mode. `smk25` clears the persisted layer state and
restarts only the SMK router. `genres` returns to engine-only Fast Mode.

All reset operations should be performed while no live notes or sustained pads
are active.

## Stable Checkpoint

The source checkpoint containing the validated runtime code and SMK setup is:

```text
performance-rig-fast-mode-stable-20260925
```
