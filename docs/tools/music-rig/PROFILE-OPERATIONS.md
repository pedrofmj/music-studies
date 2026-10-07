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

`repair-routes` is idempotent. It restores the Arturia keyboard-to-router link,
the router-to-master-control link, and a router note target for the full live
rack when no current note target is present. Use it after a restart when the
router appears green but the keyboard produces no sound. KeyLab PipeWire bridge
names can change after USB devices enumerate in a different order, including
long `Arturia KL Essential 61 mk3 at usb-...` names; the repair command resolves
the current `KL Essential 61 mk3 MIDI` port before linking it. It also waits
briefly for rack note-target ports such as `AR Controls - Sustain Scale` to
appear during startup before failing. If the Arturia selector died before
creating `s2-arturia-profile-router`, it restarts the selector once and waits
up to 90 seconds for the router ports before reconnecting the graph.

`repair-audio` restores the performance-rig output from `Arturia Main Volume
Encoder` to the selected Echora audio sink. For the analog stereo sink it also
selects the headphone/P2 port.

`recover` is the stronger recovery path for a silent rig after power cycling. It
runs the full reset sequence, waits for the KeyLab bridge, live rack ports, and
Arturia router ports to return, then runs both audio repair and route repair. In
the Echora MIDI Router Operations tab, this is exposed as `Recover Rig`.

## Post-Reboot Recovery

After `landstar` reboots with all rig cables already connected, wait 30-60
seconds after login, then use Echora MIDI Router > Performance Rig > Operations
and press `Recover Rig`. This is the normal deterministic rebuild path. It
waits for the KeyLab bridge, rebuilds the selector/router, restores the audio
route to the configured sink, and repairs the Arturia MIDI fanout.

If the rig still appears green but Arturia keys are silent, unplug and replug
only the Arturia USB cable, wait for the KeyLab to finish enumerating, then
press `Repair Routes`. USB re-enumeration can change the visible PipeWire port
name between the long `Arturia KL Essential 61 mk3 at usb-...` form and the
short `KL Essential 61 mk3 N` form; `Repair Routes` resolves the current
standard `KL Essential 61 mk3 MIDI` capture port and reconnects it to
`s2-arturia-profile-router:in`.

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
