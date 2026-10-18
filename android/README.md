# Android Standalone Rig Host

This is a standalone Android host skeleton. `MidiManager` receives USB MIDI
devices and opens BLE MIDI devices discovered by a service-UUID-filtered scan.
The BLE scan is time-bounded and can be restarted from the MIDI rescan button.
A Storage Access Framework folder selection provides the SoundFont tree; the
app copies only the profile's active SF2 files to private cache storage before
passing filesystem paths to FluidSynth. No workstation-specific SoundFont path
is assumed.

The Arturia genre and shared-SF2 route JSON are consumed as Android assets
directly from `docs/tools/music-rig/benchmarks/`. Non-SF2 catalog entries are
translated through `sf2_substitutions` before building the shared route plan.
The JNI backend compiles the portable control-state and shared-SF2 C sources
from the existing runtime, and uses the portable shared-SF2 planner to select
FluidSynth SoundFonts. SoundFonts are loaded lazily once per SAF catalog path,
then their loaded IDs and preset lists are reused across profile switches.
Each layer's requested preset is resolved by exact match against the catalog
label; missing or duplicate preset labels prevent profile activation rather
than falling back to bank/program 0/0.

The 18 cached CCs match the portable API. Startup values are zero except
control-array index 16 (Arturia knob 8, CC 16), which starts at 64. The main
encoder is not in the cache. Active profile switches configure the route and
replay each active layer's fader and knob state. Notes and uncached MIDI
messages are broadcast to active mapped channels. Faders map to the configured
volume CC and knobs to the configured reverb CC. Central encoder CC114 uses
binary-offset master volume; click CC115 toggles master mute. Both are handled
outside the 18-control cache and are not sent to individual layers.

Playback and MIDI ownership live in `RigHostService`, a foreground media
service, rather than the Activity. The Activity is only a control surface, so
screen rotation and screen-off operation do not intentionally destroy the audio
host. Notification and Bluetooth permissions still require user approval.

The first profile option is `Android Organ (single SF2 layer; not setBfree
drawbars)`. It resolves the catalog's JF Gospel Organ preset and maps Fader 1 to
that channel's volume and Knob 1 to reverb. This is an SF2 playback mode, not a
tonewheel model: the other eight faders are not drawbars and setBfree
percussion/rotary controls are not emulated. The same zero/Knob 8=64 startup
control state is used.
Bank A Pad 1 and Pad 3 select this Android Organ fallback; Bank A Pad 4 through
Bank B Pad 8 select the thirteen SF2 genre profiles. Synth Programmer (Pad 2)
is not implemented on Android yet. The SMC-Mixer, SMC-PAD, SMC-PAD Pocket,
SMK-25, and protected nine-instrument Full Live roles are also not implemented
in this Android code-only slice. Audio and MIDI are owned by `RigHostService`,
a foreground media service, for reliable screen-off operation.

## FluidSynth Dependency

No FluidSynth binaries are included. To build the JNI/audio backend, provide an
Android-compatible FluidSynth bundle with this layout:

```text
<bundle>/include/fluidsynth.h
<bundle>/lib/<android-abi>/libfluidsynth.so
<bundle>/jniLibs/<android-abi>/<FluidSynth runtime dependency libraries>
```

Configure Gradle with `-PfluidsynthRoot=/absolute/path/to/bundle`. The CMake
configuration fails explicitly if the header or ABI library is absent. The
bundle must include licenses and any transitive runtime libraries required by
the chosen FluidSynth build. Keep those runtime libraries in `jniLibs/`; the
linked `libfluidsynth.so` is imported from `lib/` by CMake. Without the
property, Android builds do not compile or package the native backend and the
app reports that the dependency is unavailable when Start is requested.

FluidSynth renders signed 16-bit stereo PCM in native code. An Android
`AudioTrack` streaming thread writes those frames to the platform audio output.
This code is an integration skeleton only; neither an APK nor the real native
audio path has been built or device-tested here.

## Mock Tests

The host, route planner, MIDI parser, and mock synthesis boundary have no
Android or FluidSynth dependency. On a machine with a JDK, run:

```sh
./android/run-mock-tests.sh
```

An Android SDK/NDK, Gradle, and the external FluidSynth bundle are additionally
needed to build the Android application and real audio backend. USB/BLE device
and physical audio testing remain follow-up work.
