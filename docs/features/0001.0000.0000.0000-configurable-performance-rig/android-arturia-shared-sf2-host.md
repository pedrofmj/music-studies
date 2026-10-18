# Android Arturia Shared-SF2 Host

Status: standalone Android source prototype added. Native FluidSynth packaging,
APK build, and device/audio verification remain pending.

## Implementation

The app receives USB and BLE MIDI through Android `MidiManager`, parses running
status, and uses the shared Arturia CC/profile data. SoundFont locations are
selected through the Storage Access Framework and materialized into private
cache storage; no workstation-specific path or SoundFont binary is stored in
Git. A FluidSynth JNI backend loads each SF2 path once per app session, resolves
the catalog preset label to a real SF2 bank/program, and reuses loaded SoundFonts
across profile switches. `AudioTrack` provides the stereo output stream.

Startup control state is faders `0`, knobs `0` except Knob 8 `64`. Fader/knob
CCs are cached and replayed to each new profile. Main encoder volume and mute
remain separate. Bank A Pad 1 selects the Android Organ profile; Pad 4 through
Bank B Pad 8 select the SF2 genre profiles. The app includes a profile picker as
well, so device-free testing does not require MIDI input.

## Known Differences And Gates

- Android Organ uses one JF Gospel Organ SF2 preset. It is not setBfree, has no
  nine drawbars, and does not implement setBfree percussion or rotary controls.
- Synth Programmer, the protected nine-instrument Full Live Rack, and SMC/SMK
  controller roles are not implemented. Pad 2 is unavailable; Pad 3 returns to
  the Android Organ fallback rather than Linux Full Live.
- `RigHostService` owns playback and MIDI in a foreground media service so the
  Activity is only a control surface during screen-off/background operation.
- The native FluidSynth Android bundle is external. It must contain compatible
  per-ABI libraries and license notices; the build fails closed without it.
- Preset labels are matched exactly and profile activation fails if the
  selected SF2 does not contain an unambiguous matching preset.
- Java mock tests, Gradle/NDK build, APK packaging, USB/BLE MIDI, and audio
  playback remain unverified in this code-only slice.

No Android test device was opened or modified during implementation.
