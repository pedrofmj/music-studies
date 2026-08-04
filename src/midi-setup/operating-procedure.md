# Operating Procedure

## Normal Startup

1. Connect and power the controller before opening Carla.
2. Confirm PipeWire and WirePlumber are active with
   systemctl --user is-active pipewire wireplumber.
3. Launch `~/bin/carla-pedro-project`. It forwards
   `/c/music/carla/pedro.uproject` into the Flatpak and loads the rack during
   Carla startup. Avoid opening a blank Carla instance and then using Open for
   this project; the multi-instance rack can hang during in-process reload.
4. In Carla's Patchbay, confirm the KeyLab MIDI port feeds MIDI Scale CC Value
   and that the LSP Mixer outputs feed the intended audio sink.
5. Play a note and operate the sustain pedal before adding or changing a
   plugin.

The project stores its ExternalPatchbay connections, so Carla is responsible
for restoring this performance graph. qpwgraph is best used to inspect failed
restoration or a changed device name, not to duplicate the normal links.

## Adding The New Plugins

Save the current project first. In Carla, open the Add Plugin dialog, refresh
the LV2 scan, and search for Surge XT and Dragonfly. The required bundles are
already visible to Carla through its working document-portal LV2 path.

For Surge XT, add the instrument and connect its MIDI input to MIDI Scale CC
Value; send its stereo outputs to unused LSP Mixer inputs. For Dragonfly,
start with an instrument-specific or send/return reverb path. Do not insert a
fully wet reverb after the LSP master output by default, because that affects
every existing layer.

Save a new project revision after a successful audio and pedal test. Keep the
working pedro.uproject unchanged until the new revision is confirmed.

## qpwgraph Recovery Use

Open qpwgraph only after Carla and the controllers are running. It should show
the same KeyLab-to-filter and mixer-to-sink links documented in
[Current State](current-state.md). If a controller re-enumerates with a new
port name, repair that one link, test it, then save the Carla project so its
ExternalPatchbay records the corrected endpoint.

Do not configure qpwgraph to persist a second copy of the entire Carla rack.
Use a qpwgraph patchbay profile only for links that deliberately live outside
the Carla project, such as a controller-to-standalone-application connection.

## Backup And Diagnostics

Before modifying the rack, create a dated copy of the project:

~~~bash
cp -a /c/music/carla/pedro.uproject \
  /c/music/carla/pedro-$(date +%F-%H%M%S).uproject
~~~

Useful non-destructive checks on airstar are:

~~~bash
pw-link -iol
wpctl status
flatpak info studio.kx.carla
dpkg-query -S /usr/lib/lv2/'Surge XT.lv2'/manifest.ttl
~~~

If a package-installed LV2 plugin is absent from Carla, first confirm that the
existing document-portal LV2 path is still present in
~/.var/app/studio.kx.carla/config/falkTX/Carla2.conf. Do not add a direct
/usr/lib/lv2 Flatpak override; it is rejected because /usr is reserved by the
Flatpak runtime.
