# Operating Procedure

## Normal Startup

1. Connect the Arturia and all four M-VAVE controllers.
2. Confirm PipeWire, WirePlumber, and the four setup services are active.
3. Launch ~/bin/carla-pedro-project. Do not open a blank Carla instance and
   then load this large project through Open; direct project startup avoids
   the previously observed in-process reload hang.
4. Wait for all SoundFonts to load.
5. Play the Arturia, SMK-25, and pad devices and confirm Carla meters move.
6. Run the live validator from the repository:

~~~bash
docs/tools/airstar-live-setup/validate-airstar-live-setup --live --fast
~~~

## Configurable Rig Development

The normal startup above remains the production workflow. Experimental
Performance Rig services and profiles stay disabled unless a milestone
procedure explicitly enables them.

Before any experiment, run the read-only protection check:

~~~bash
docs/tools/airstar-live-setup/verify-protected-baseline
docs/tools/airstar-live-setup/restore-protected-baseline
~~~

The second command is only a preview unless `--apply` is supplied.

## Intentional Routing Changes

After changing a live connection and confirming audio:

~~~bash
~/bin/pipewire-patchbay-refresh
~~~

That command takes a new full snapshot and restarts the event-driven restore
service. It affects the machine's runtime snapshot, not the Git artifacts.

To inspect restoration without changing links:

~~~bash
~/bin/pipewire-patchbay-json --check-and-restore --dry-run
~~~

## Versioning A Confirmed Change

Save the Carla project, refresh the live snapshot, then run locally:

~~~bash
docs/tools/airstar-live-setup/capture-from-airstar --update-protected-baseline
~~~

Review the project, raw snapshot, MIDI snapshot, deployment snapshot,
project.json, and setup.json changes together. The capture refuses an
unexpected plugin or connection count so structural changes require a
deliberate manifest update.

## New Machine

Provision the external SoundFont and DecentSampler assets, apply the controller
profiles, then run:

~~~bash
docs/tools/airstar-live-setup/install-airstar-live-setup
~~~

Use --replace-snapshot only when the versioned deployment graph should replace
an already configured machine's snapshot.

## Recovery

If devices disappear, reconnect the USB root and allow two seconds for the
event watcher. If links remain missing, run the dry check above. Do not refresh
the snapshot while devices or Carla ports are absent, because that would make
the incomplete graph authoritative.

If Carla is unresponsive, close it and relaunch the project through
~/bin/carla-pedro-project. The helper services and event watcher can remain
running; their named ports will reconnect when Carla returns.

To restore the complete protected single-rig deployment, first preview the
operation:

~~~bash
docs/tools/airstar-live-setup/restore-protected-baseline
~~~

Close Carla and apply the restore only when recovery is required:

~~~bash
docs/tools/airstar-live-setup/restore-protected-baseline --apply
~~~

The command keeps timestamped pre-restore evidence and does not launch Carla.
After restoration, use the normal startup and live validation procedure.

Before manual project replacement, keep a timestamped backup:

~~~bash
cp -a /c/music/carla/pedro.uproject \
  /c/music/carla/pedro-$(date +%F-%H%M%S).uproject
~~~
