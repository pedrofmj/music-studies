# Arturia Profile Session

The Arturia pad selector is an explicit systemd **user** service. It is not
enabled by the installer and does not start at login.

## Install

Stage the candidate resources on the performance laptop, then run from the
repository:

```bash
docs/tools/music-rig/packaging/linux/install-arturia-profile-session \
  --candidate-root /tmp/music-rig-synthv1-ubuntu-20260912
```

The installer copies the router, mapped synthv1 preset/config, setBfree binary,
setBfree config, and libraries below
`~/.local/share/music-rig/arturia-profile-session`. It installs
`music-rig-arturia-profile-session.service` under
`~/.config/systemd/user`, reloads the user manager, and does not enable or start
the unit.

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
