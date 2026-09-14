# Arturia Profile Session

The Arturia pad selector is an explicit systemd **user** service. It is not
enabled by the installer and does not start at login.

## Install

Stage the candidate resources on the performance laptop, then run from the
repository:

```bash
docs/tools/music-rig/packaging/linux/install-arturia-profile-session \
  --candidate-root /tmp/music-rig-synthv1-ubuntu-20260912 \
  --genre-project-root /path/to/materialized/genre-projects
```

The installer copies the router, mapped synthv1 preset/config, setBfree binary,
setBfree config, and libraries below
`~/.local/share/music-rig/arturia-profile-session`. It installs
`music-rig-arturia-profile-session.service` under
`~/.config/systemd/user`, reloads the user manager, and does not enable or start
the unit.

Materialize genre projects first with
`benchmarks/materialize-arturia-genre-project.py` using
`benchmarks/arturia-genre-patches.json`. The materializer reads the protected
project as a template and writes only candidate `.uproject` files.

## Operate

```bash
systemctl --user start music-rig-arturia-profile-session.service
systemctl --user status music-rig-arturia-profile-session.service
systemctl --user stop music-rig-arturia-profile-session.service
```

The service starts in `full-live-rack`, consumes only the Arturia channel-10 pad
management notes, forwards keyboard/control MIDI, and swaps only the Arturia
audio layer. SMK-25, SMC-Mixer, SMC-PAD, and SMC-PAD Pocket remain connected.
Stopping or failing the service restores the default Arturia layer.

## Transition Certification

Run certification only on a performance station with no one pressing physical
pads during the run. The test sends management bytes through the selector FIFO
and checks the live engine, selector, PipeWire sink, MIDI links, shared
LSP/SMC graph, and master output links after every transition.

Preflight the station:

```bash
systemctl --user start music-rig-pedro-carla-headless.service
systemctl --user start music-rig-arturia-profile-session.service
wpctl status -n
```

Run a deterministic random permutation:

```bash
python3 docs/tools/music-rig/benchmarks/certify-pad-transitions.py \
  --control-fifo "$HOME/.local/state/music-rig/arturia-profile-session/profile.fifo" \
  --seed 20260915 \
  --timeout 100
```

The test covers Pads 1 through 16 exactly once, then restores Pad 3 Full Live.
Use `--sequence` with a complete permutation when reproducing a specific run.
The command exits nonzero if any transition fails or if the selector stops.

Certification requires all of the following:

- Every result reports `"status": "pass"`.
- The selector remains active throughout.
- The real ALSA sink is selected; `auto_null` must not be the default sink.
- Full Carla is active for Pads 1 through 3 and inactive during genre modes.
- The expected engine or genre project is registered.
- MIDI, LSP, SMC, master-volume, and hardware-output links are present.
- The final state is Full Live with the selector active.

The test validates graph and runtime safety; it does not replace a short manual
keyboard rehearsal after certification.

## Diagnostics

The selector appends structured transition events to:

```text
~/.local/state/music-rig/arturia-profile-session/transition-events.jsonl
```

The systemd unit also writes a stop/crash snapshot containing its result,
recent journal, PipeWire metadata, and the complete port graph to:

```text
~/.local/state/music-rig/arturia-profile-session/reports/
```

Collect the latest report after an incident with:

```bash
python3 -c 'from pathlib import Path; print(max((p for p in (Path.home()/".local/state/music-rig/arturia-profile-session/reports").glob("*.log")), key=lambda p: p.stat().st_mtime))'
journalctl --user -u music-rig-arturia-profile-session.service -n 200 --no-pager
```

Keep the matching `transition-events.jsonl` file with the report.

Uninstall without enabling anything:

```bash
docs/tools/music-rig/packaging/linux/install-arturia-profile-session \
  --uninstall
```

The service is deliberately not a production default until an interactive
performance rehearsal confirms startup, pad switching, and shutdown recovery.

## Optional Headless Full Rack

Install the separate headless Carla launcher and user unit:

```bash
docs/tools/music-rig/packaging/linux/install-pedro-carla-headless
```

It is installed disabled and stopped. Close the normal GUI Carla rack before
starting it:

```bash
systemctl --user start music-rig-pedro-carla-headless.service
~/.local/bin/carla-pedro-osc-gui
```

`carla-pedro-osc-gui` uses Carla's bundled OSC control frontend to attach to the
headless backend. It does not launch a second Carla engine. Stop the backend
before reopening the normal GUI rack.
