# Android Standalone Rig Host

`MidiManager` receives USB MIDI devices and opens BLE MIDI devices discovered
by a service-UUID-filtered scan. The BLE scan is time-bounded and can be
restarted from the MIDI rescan button. SF2 files are fetched from the versioned
HTTPS pack only when the selected profile needs them, verified against the
published size and SHA-256, and kept in app-private storage. A confirmed
prefetch action can download the complete 495 MB Android pack.

The Arturia genre, shared-SF2 route, and Android override JSON files are copied
as small build-time assets from `docs/tools/music-rig/benchmarks/`. Non-SF2
catalog entries are translated through `sf2_substitutions` before building the
shared route plan. The Android override maps the selected catalog entries to
the paths published in the cloud pack.
The JNI backend compiles the portable control-state and shared-SF2 C sources
from the existing runtime, and uses the portable shared-SF2 planner to select
FluidSynth SoundFonts. SoundFonts are loaded lazily once per catalog path, then
their downloaded files, loaded IDs, and preset lists are reused across profile
switches. No SF2 binary is embedded in the AAB.
Each layer's requested preset is resolved by exact match against the catalog
label; missing or duplicate preset labels prevent profile activation rather
than falling back to bank/program 0/0.

Only SoundFonts referenced by the selected profile are loaded. Loaded IDs and
preset indexes are retained for later switches, avoiding an eager load of the
entire catalog into the long-lived audio process.

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

No third-party binaries are committed. The JNI/audio backend is built against
the upstream FluidSynth 2.6.0 Android 24 release bundle. Download and verify it
outside the repository before building:

```sh
curl -fL -o /tmp/fluidsynth-v2.6.0-android24.zip \
  https://github.com/FluidSynth/fluidsynth/releases/download/v2.6.0/fluidsynth-v2.6.0-android24.zip
echo "81cd70dddb3381cda02c430a6657923766d1acb83de11d266d7a19aee3c7ca4d  /tmp/fluidsynth-v2.6.0-android24.zip" | sha256sum -c
mkdir -p /tmp/fluidsynth-v2.6.0-android24
unzip -q /tmp/fluidsynth-v2.6.0-android24.zip -d /tmp/fluidsynth-v2.6.0-android24
gradle -p android bundleDebug \
  -PfluidsynthRoot=/tmp/fluidsynth-v2.6.0-android24
```

The archive supplies `include/fluidsynth.h` and `lib/<abi>/*.so` for
`arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`. Gradle packages the shared
dependencies from the same ABI directories, and CMake links the imported
`libfluidsynth.so`. The shared C++ runtime is enabled for the JNI target because
the upstream FluidSynth library depends on `libc++_shared.so`. The native
configuration fails explicitly if the header or ABI library is absent.

The upstream artifact is built for Android API 24; the application `minSdk` is
therefore 24 (Android 7.0). Supporting API 23 requires rebuilding FluidSynth
and its dependencies for that API rather than reusing this binary.

FluidSynth and libsndfile are LGPL-2.1-or-later; the bundled Oboe is Apache-2.0;
the FLAC, Ogg, Opus, and Vorbis libraries use BSD-style licenses. The NDK's
`libc++_shared` has the LLVM/libc++ license terms. The upstream archive does not
contain these license texts. Before distributing an app bundle, include the
applicable notices and satisfy the LGPL source/relinking terms for the exact
upstream libraries shipped. Without `-PfluidsynthRoot`, Android builds omit the
native backend and the app reports that the dependency is unavailable when
Start is requested.

## Cloud SF2 Pack

The Android pack root is
`https://echora.egypteam.com/downloads/music-rig/sf2/v1/android/`. The app
downloads `asset-index.tsv` on first use, caches the versioned index, and fetches
only the SHA-256-named SF2 assets referenced by the active profile. Each asset
is streamed to a temporary file, checked against the index size and digest, then
atomically published in private app storage. Invalid, partial, or mismatched
downloads are not exposed to FluidSynth. The full-pack button runs in a
foreground data-sync service and reports verified-file progress in its
notification.

FluidSynth renders signed 16-bit stereo PCM in native code. An Android
`AudioTrack` streaming thread writes those frames to the platform audio output.
This code is an integration skeleton. Building an AAB verifies compilation and
packaging only; physical audio and MIDI behavior still requires device testing.

## Mock Tests

The host, route planner, MIDI parser, pack-index parser, download verifier, and
mock synthesis boundary have no Android or FluidSynth dependency. On a machine
with a JDK, run:

```sh
./android/run-mock-tests.sh
```

To also retrieve a small SF2 from the live server and validate it through the
same HTTPS/index/hash path, set the live pack root:

```sh
MUSIC_RIG_LIVE_SF2_URL=https://echora.egypteam.com/downloads/music-rig/sf2/v1/android/ \
  ./android/run-mock-tests.sh
```

An Android SDK/NDK, Gradle 8.7, and the verified external FluidSynth bundle are
needed to build the Android application and real audio backend. USB/BLE device
and physical audio testing remain follow-up work.
