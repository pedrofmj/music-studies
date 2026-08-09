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
docs/tools/airstar-live-setup/capture-from-airstar
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

Before manual project replacement, keep a timestamped backup:

~~~bash
cp -a /c/music/carla/pedro.uproject \
  /c/music/carla/pedro-$(date +%F-%H%M%S).uproject
~~~
