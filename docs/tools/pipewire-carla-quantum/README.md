# PipeWire Carla Quantum

This user service forces a 1024-frame PipeWire graph quantum for Pedro's Carla
live rack on `airstar`. At 48 kHz this is a 21.3 ms processing period.

The setting was revalidated against the current protected 49-plugin rack on
2026-08-19:

- 1024 frames produced no audible noise or new PipeWire errors during a
  one-minute 8-note, layered-pad, and drum stress test. The player reported a
  clear latency improvement over 2048 frames.
- 512 frames improved latency further, but the sink added one error and
  `AR-CH-8 - PAD EFEITOS` added two processing errors during the same class of
  stress test. It is not the production setting.

This supersedes the 2026-08-08 result from the earlier manually built rack,
where 1024 frames added sink deadline errors and 2048 frames was required for
clean playback. Polyphony remains unrestricted.

The 2026-08-19 scheduling audit found the performance power profile and Intel
EPP performance policy active, turbo enabled, RTKit active, and the PipeWire
and Carla data loops running `SCHED_RR` at priority 20. No scheduler setting was
changed. Further 512-frame work should target measured plugin workload.

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
The 2026-08-19 live trial reconfirmed that behavior. Apply a different value
with Carla closed, then reopen the rack through `~/bin/carla-pedro-project`.
