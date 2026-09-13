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
