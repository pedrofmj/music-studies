# Airstar Live Setup Deployment

This directory is the reproducible Linux deployment layer for Pedro's Carla
performance rack. It combines the versioned project, controller mappings,
PipeWire graph, helper services, operating-system dependencies, and external
asset fingerprints into one installable definition.

## Protected Production Baseline

The versioned single-rig setup is the production default while Configurable
Performance Rig development is in progress. Experimental code does not replace
its project, Patchbay snapshot, services, controller helpers, startup command,
or state.

Verify the protected source artifacts at any time. This command is read-only:

~~~bash
docs/tools/airstar-live-setup/verify-protected-baseline
~~~

Preview the recovery procedure, also without changing anything:

~~~bash
docs/tools/airstar-live-setup/restore-protected-baseline
~~~

Only an intentional recovery uses:

~~~bash
docs/tools/airstar-live-setup/restore-protected-baseline --apply
~~~

The apply operation requires Carla to be closed, saves timestamped copies of the
currently deployed project, graph snapshot, configuration, and service evidence,
then reinstalls the protected setup. It does not launch Carla automatically.

## Owned By Git

- setup.json is the machine-readable source of truth for the Ubuntu baseline,
  packages, hardware roles, controller messages, assets, Carla structure,
  services, quantum, and graph checksums.
- protected-baseline.json identifies and fingerprints every artifact required
  to restore the production single-rig setup.
- verify-protected-baseline validates those source artifacts without touching
  the installed machine.
- restore-protected-baseline previews or explicitly restores the protected
  deployment while preserving pre-restore evidence.
- install-airstar-live-setup installs packages, Carla, the project, launchers,
  helper services, performance quantum, and the saved graph.
- validate-airstar-live-setup checks the installed machine and optionally its
  connected, running graph.
- materialize-carla-project.py relocates absolute home and SoundFont paths
  while preserving the project structure.
- materialize-patchbay.py maps the reference output links to the target machine's current default stereo sink.
- capture-from-airstar refreshes the versioned project and graph artifacts
  after an intentional live-rack change.
- capture-control-xrun-correlation records timestamped MIDI arrivals and
  PipeWire xrun/error-counter deltas without changing links, services, Carla,
  EQ parameters, or quantum.
- [Controller Profiles](controller-profiles.md) records the hardware settings
  that must be applied in Arturia software or CubeSuite.

The component implementations remain in their focused tool directories:
[Carla rack](../carla-live-rack/README.md),
[Arturia encoder](../arturia-main-volume-encoder/README.md),
[SMK layers](../smk25-pad-layers/README.md), and
[PipeWire quantum](../pipewire-carla-quantum/README.md).

## External Assets

Git does not contain the 49 GB SoundFont library, DecentSampler binary, or
DecentSampler libraries. A target needs legitimate copies in these standard
locations before installation:

~~~text
~/Flash/PED/MIDI/Pack de Timbres/Library
~/.vst/DecentSampler.so
~/.config/DecentSampler/Sample Libraries/Basic Piano.dsbundle
~/.config/DecentSampler/Sample Libraries/DS + VT - altstrings Free Edition.dsbundle
~~~

The Ubuntu fluid-soundfont-gm package provides the flute bank; the installer
copies it into Carla's user-visible path. setup.json records exact sizes and
SHA-256 fingerprints for all 17 direct assets and deterministic tree
fingerprints for the two DecentSampler bundles.

These fingerprints prove that a target has the same inputs. They do not grant
redistribution rights. A copy prepared for a friend or a sale must include
only assets and plugins whose licenses permit that distribution.

## Install A Matching Linux Machine

1. Install the standard Ubuntu 24.04 image and create or mount /c with the
   target user allowed to write /c/music/carla.
2. Put the SoundFont pack, DecentSampler VST2, and its two libraries at the
   paths above.
3. Apply [Controller Profiles](controller-profiles.md) in the hardware editors.
4. Clone this repository and run:

~~~bash
docs/tools/airstar-live-setup/install-airstar-live-setup
~~~

The installer is idempotent. It preserves an existing Patchbay snapshot unless
--replace-snapshot is supplied. To inspect a machine without changing it:

~~~bash
docs/tools/airstar-live-setup/install-airstar-live-setup --check-only --fast
~~~

A different Linux username works because project paths are materialized under
the current HOME. The final stereo links are also mapped to the target
machine's current PipeWire default output instead of assuming the Dell sink. A nonstandard SoundFont location is also supported:

~~~bash
docs/tools/airstar-live-setup/install-airstar-live-setup \
  --soundfont-root "/data/PED/MIDI/Pack de Timbres/Library"
~~~

After installation, connect all five controllers, launch the rack, wait for
the SoundFonts to load, and run:

~~~bash
~/bin/carla-pedro-project
docs/tools/airstar-live-setup/validate-airstar-live-setup --live --fast
~~~

The live check requires four M-VAVE USB devices, the Arturia, Carla, all four
services, the 1024-frame quantum, and every saved link.

## Graph Artifacts

- src/midi-setup/airstar-patchbay.json is the raw 118-link reference-host
  snapshot.
- src/midi-setup/airstar-midi-patchbay.json is its 68-link MIDI-only inventory.
- src/midi-setup/pedro-live-rack-patchbay.json is the 116-link deployment
  snapshot.

The deployment snapshot excludes two ALSA Playback [java] links that happened
to share the system sink but are not part of the performance rack. No
controller, instrument, mixer, EQ, volume gate, or system-output link is
removed.

## Capturing Later Changes

Save the Carla project and refresh its live Patchbay first. From this repository
run:

~~~bash
docs/tools/airstar-live-setup/capture-from-airstar --update-protected-baseline
~~~

The command fetches /c/music/carla/pedro.uproject, exports the full and
MIDI-only live graphs, rebuilds the curated deployment graph, validates the
49-plugin/111-connection contract, and updates checksums. The required flag
exists because this operation replaces protected artifacts; it is not used for
experiments or diagnostics. Review and commit the result, then deliberately
update the protected-baseline fingerprints. If the rack gains or loses plugins,
update the manifest and validators rather than bypassing the structural check.

## Control And Xrun Correlation

Run this observer while an operator moves the SMC-Mixer. It is read-only; it
uses temporary remote files and removes them before returning. MIDI timestamps
are observer arrival times grouped into one-second UTC buckets, not device timestamps:

~~~bash
docs/tools/airstar-live-setup/capture-control-xrun-correlation \
  --duration 60 \
  --midi-ports 20:1 \
  --output-json /tmp/airstar-control-xrun.json
~~~

The report compares the protected Patchbay hash before and after, records
`pw-top` changed-node counters, and captures xrun/dropout/deadline journal
matches. It never connects or disconnects ports and does not change Carla,
parameters, services, or PipeWire quantum.
Its `correlation.per_second` array joins the MIDI arrival buckets to the
per-node PipeWire ERR snapshots and deltas for each observation second.

## Windows Boundary

The manifest's instruments, CC mappings, controller modes, asset identities,
and routing intent are platform-neutral. The current executable adapters are
not: they use PipeWire/JACK, systemd user services, apt, and Linux VST2/LV2
binaries.

A future Windows package should keep setup.json as the behavioral contract and
provide Windows adapters for MIDI discovery/routing, background lifecycle,
Carla/plugin installation, audio-buffer configuration, and path
materialization. Echora or Galaxy SDK integrations can replace those adapters
when ready without changing the controller behavior documented here.
