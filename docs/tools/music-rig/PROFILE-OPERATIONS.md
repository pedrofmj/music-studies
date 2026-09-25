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
music-rig-profile events --limit 30
```

`apply` saves the previous environment, restarts the selector, waits for the
expected warm-genre event, and restores the previous environment if readiness
fails.

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
