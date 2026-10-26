# Arturia Cross-Platform SoundFont Hosts

Status: code-only foundations are in progress. No Windows or Android MIDI/audio
device has been opened or certified by this work.

## Goal

Carry the Linux shared-SF2 fast profile model to Windows and Android while
keeping profile meanings and Arturia control values consistent. Insync remains a
development/build-time source; distribution uses one self-contained SF2 pack,
separate from application binaries and source control. Runtime package-root
integration remains in progress.

The shared control contract is 18 channel-1 CCs: nine faders followed by nine
instrument knobs. A fresh selector session starts in Organ with all faders at
CC value 0 and all knobs at 0 except Knob 8 at 64. The main encoder (CC114/115)
is separate. Each received fader/knob value updates session state and profile
changes replay that state through the target profile's mapping.

## Shared-SF2 Model

`arturia-genre-patches.json` remains the source for the 13 nine-slot genre
catalog. `arturia-shared-sf2-routing.json` records the common layer CC maps,
per-genre active layers, and substitutions for DecentSampler-only patches. The
portable C route planner compacts active layers into mixer slots and carries the
patch index and fader/knob CC identity to a platform host.

Windows Carla and Android FluidSynth are host adapters over this same profile
and control model; host/plugin APIs and audio drivers are platform-specific.
The shared catalog keeps source-library-relative paths. A packager stages a
content-addressed pack and records hashes and license notices. An offline Carla
project materializer verifies the pack and rewrites only SF2 filenames/labels in
a caller-provided project, preserving its patchbay. Android keeps the route JSON
shared but may apply `arturia-sf2-assets.android.json` for patch-ID-specific
assets and preset labels; its host hash-verifies selected files in app-private
cache. Store delivery and the actual pack still require asset and license review.

## Platform Status

### Windows

The Windows code-only slice adds a WinMM MIDI adapter with stable device-interface
identity, bounded input/output queues, control-state snapshot/replay helpers,
and a fake-device offline test. The shared route planner is portable C. Carla
project hosting, native ASIO/WASAPI output integration, packaged-SF2 runtime
resolution, and physical-device verification remain incomplete. The existing
Insync path resolver is an offline-tested utility and is not used by the
production Carla lifecycle.

### Android

The Android code-only slice adds a standalone app using Android `MidiManager`,
optional bundled-pack or Storage Access Framework SoundFont sources, a FluidSynth
JNI backend, and `AudioTrack`. The host lazily extracts only selected packed SF2s
to app-private cache and verifies them against the Android asset index; loaded
IDs and preset lists are reused across profile switches. A separately supplied,
correctly licensed FluidSynth Android bundle is required to build the native
audio backend. Store delivery and the selected replacement files remain pending.

Android Organ currently uses one JF Gospel Organ SF2 preset as an explicitly
labeled approximation. It is not setBfree and does not provide nine drawbars,
rotary/percussion controls, the Synth Programmer mode, Full Live, or the SMC/SMK
roles. These are open implementation gaps, not certified behavior.

## Code-Only Verification

The shared route and control-state tests use synthetic patches and MIDI data.
The WinMM fake seam does not enumerate physical devices. Android mock tests do
not start FluidSynth or open Android MIDI/audio APIs. These checks establish
software contracts only; they do not establish device identity, audible parity,
startup latency, load behavior, stability, resource limits, or production
readiness.
