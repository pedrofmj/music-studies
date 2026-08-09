# PipeWire Carla Quantum

This user service forces a 2048-frame PipeWire graph quantum for Pedro's Carla
live rack on `airstar`. At 48 kHz this is a 42.7 ms processing period.

The setting was selected from live complex-layer tests on 2026-08-08:

- 512 frames produced frequent audible noise and deadline spikes.
- 1024 frames improved playback but added 12 sink deadline errors during the
  test passage.
- 2048 frames produced no audible noise, no perceived playing delay, and no
  additional sink deadline errors during the same passage.

Install and enable it with:

```bash
docs/tools/pipewire-carla-quantum/install-pipewire-carla-quantum
```

Inspect the active value with:

```bash
pw-metadata -n settings 0 | grep clock.force-quantum
pw-top
```

Return to automatic quantum selection with:

```bash
systemctl --user disable --now pipewire-carla-quantum.service
pw-metadata -n settings 0 clock.force-quantum 0
```

Changing the quantum while Carla is running causes this Carla build to exit.
Apply a different value with Carla closed, then reopen the rack through
`~/bin/carla-pedro-project`.
