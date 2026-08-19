# Protected Helper Offline Tests

Status: Milestone 0 offline parity evidence captured on 2026-08-10. These tests
do not install, start, stop, restart, or connect either helper.

## Safety Boundary

The tests compile the protected Arturia and SMK-25 sources directly from their
repository paths. They do not modify those sources or their deployment files.

The Arturia test supplies process-local JACK mocks and temporary audio/MIDI
buffers. Its state round trip uses a uniquely named file under `/tmp` and
removes it before exiting. It never loads `libjack` and cannot open a JACK
client.

The SMK-25 binary links a test-only forbidden-JACK stub instead of the JACK
runtime. Every stubbed JACK entry point terminates the process with a failing
exit code, so a test cannot silently connect even if argument handling later
regresses. The two tested command paths make no JACK calls:

- `--self-test` exercises the source's built-in event/state harness; and
- `--check-config` reads and validates the protected mapping file.

No installer, systemd command, user service, production state path, MIDI port,
audio port, or PipeWire graph operation is invoked.
An additional guard test requires the normal startup path to terminate at the
stubbed `jack_client_open` with exit code 99 and create no state file.
Both resulting helper test binaries link only to libc; neither links to
`libjack`.

## Covered Behavior

The external Arturia harness verifies:

- MIDI value clamping and missing-state defaults;
- atomic volume/mute state persistence through a temporary file;
- absolute CC119 volume replay on a new output connection;
- one channel-10 CC7=127 drum reset on connection/reconnection, with no steady
  connection repeats;
- channel-1 relative CC114 conversion and neutral/wrong-channel rejection;
- edge-triggered CC115 mute/unmute output on CC118;
- one generation publication per accepted control edge; and
- bounded, synchronized stereo mute and unmute ramps.

The existing SMK-25 self-test verifies:

- per-layer knob routing and retained knob state;
- pad/channel matching and layer enable/disable;
- latched chord replacement and note-release behavior;
- Stop/Play pause and resume semantics;
- transport-event consumption; and
- default chord seeding.

The configuration test additionally validates the captured protected SMK-25
mapping.

## Results

| Build | Result |
| --- | --- |
| GCC 13.3.0, normal build | 12/12 tests passed |
| Clang 18.1.3, normal build | 12/12 tests passed |
| GCC 13.3.0, JSON probe enabled | 13/13 tests passed |
| Clang 18.1.3, JSON probe enabled | 13/13 tests passed |
| Protected-baseline verifier | 30/30 checks passed |

## Reproduction

~~~bash
cmake -S docs/tools/music-rig -B /tmp/music-rig-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/music-rig-build
ctest --test-dir /tmp/music-rig-build --output-on-failure
ctest --test-dir /tmp/music-rig-build -V \
  -R 'music_rig_(arturia|smk25)_helper'
~~~

The helper test binaries have no runtime dependency on JACK.

## Remaining Live Evidence

Offline parity does not prove hardware discovery, physical encoder/pad input,
controller LED state, PipeWire reconnection, service restart recovery, or
audible ramp quality. Those checks remain part of the repeatable current-rack
startup transcript and later explicit live-hardware procedure. They must not be
run automatically. The 2026-08-18 guarded Airstar repair separately observed the
deployed initializer emit channel-10 CC7 value 127 and then removed its temporary
observer link.
