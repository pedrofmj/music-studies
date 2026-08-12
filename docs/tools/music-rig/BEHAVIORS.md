# Reusable Current Behaviors

## Purpose

`music_rig_current_behaviors` extracts the deterministic mapping and state
transitions used by the protected Arturia and SMK-25 services. It gives the
future device/MIDI adapter a portable compatibility reference without changing,
linking into, installing, stopping, or replacing either legacy service.

The library has no main loop and opens no MIDI, audio, graph, filesystem, IPC,
or service resource. It only mutates caller-owned state and returns decisions or
invokes a caller-supplied synchronous event callback. The current tests use only
bounded in-memory capture callbacks, so building or testing this library cannot
emit an event to the live Rig.

## Ownership And Performance

Both behaviors are initialized once with a copied configuration and then owned
by one execution context. The configuration is immutable after initialization.
Cross-thread transfer or inspection must be added by an adapter using the
runtime's established immutable-generation boundary; these structures are not
implicitly synchronized.

The event path has:

- no allocation, deallocation, lock, platform API, filesystem API, JSON
  traversal, or string comparison;
- constant work for Arturia MIDI events and each Arturia audio frame;
- fixed work bounded by eight layers and 128 notes for SMK-25 note, transport,
  and latch transitions; and
- a synchronous emit callback whose storage and overflow policy belong to the
  adapter.

Compile-time ceilings reject growth beyond 64 bytes for Arturia behavior, 16
bytes for its snapshot, 4,096 bytes for SMK-25 behavior, and 2,048 bytes for its
snapshot. On the current 64-bit Linux GCC build the corresponding sizes are 24,
8, 2,352, and 1,052 bytes.

## Arturia Contract

The default configuration reproduces MIDI channel 1, relative volume input CC
114, mute-button input CC 115, mute output CC 118, and absolute volume output CC
119. The behavior preserves:

- binary-offset relative volume, including clamping and neutral-value handling;
- rising-edge mute toggling and held/release behavior;
- stored-volume replay when an output connection appears;
- generation changes at the same transitions as the protected service;
- the current stereo mute-gate ramp, one audio frame per call; and
- bounded import of the current `volume mute` text state.

The API returns at most one three-byte MIDI decision per input transition. It
does not send that decision anywhere.

## SMK-25 Contract

The default configuration reproduces the current eight CC pads across MIDI
channels 1 through 8, eight knobs on channel 1, Play note 94, Stop note 93, and
the 48/52/55 fallback chord. The behavior preserves:

- value and toggle pad modes;
- eight independent layer enable, pause, and pad-edge states;
- new-gesture chord replacement and enabled-layer note latching;
- Play, Stop, MIDI realtime, MMC, and all-notes-off behavior;
- per-layer knob routing and stored-value replay on new output connections;
- passthrough to all eight layer outputs; and
- bounded import of the current `SMK25_PAD_LAYERS 1` text state.

The emit callback receives the target layer, frame, and immutable message view
synchronously. A shadow adapter must record or compare that decision without
forwarding it to a backend output.

## State Boundary

The versioned snapshot structures contain the logical state required for
restart compatibility. They are in-process structures, not serialized disk or
IPC frames: native padding, `bool`, and `size_t` layout must never become a
persistent multiplatform ABI. A later state integration must encode their fields
through an explicit byte format before using the runtime storage adapter.

Legacy text import exists only to compare and migrate current state. It performs
no file I/O; callers supply a bounded memory view. Runtime-only physical-note,
active-note, button-edge, audio-ramp, and connection-count state is intentionally
reinitialized or kept outside the persisted logical snapshot as applicable.

## Verification And Safety

The portable unit suite covers current defaults, malformed input, Arturia
relative volume/mute/audio/connection behavior, SMK-25 pad/knob/latch/transport/
connection behavior, alternate toggle mode, legacy import, and snapshot restore.
A source audit rejects allocation, C locks, platform headers, backend names,
thread APIs, and filesystem calls from the complete behavior tree.

Linux-only differential tests compile the protected sources into offline test
processes. They compare the extracted Arturia state, output, connection replay,
and audio samples and the extracted SMK-25 emissions, complete state, connection
replay, Play/Stop behavior, and legacy-state recall. The protected source files
remain unchanged and the forbidden JACK stub terminates a test if the SMK-25
fixture attempts any backend call.

Local GCC and Clang pass 47/47 tests, both optional JSON builds pass 50/50,
twelve selected ASan/UBSan behavior and runtime boundaries pass, and the
protected baseline remains 30/30. Hosted
[run 31551820903](https://github.com/pedrofmj/music-studies/actions/runs/31551820903)
passes Linux 47/47, Windows 36/36, and Windows JSON 39/39; both Windows builds
compile the library and execute its portable unit and boundary tests under
strict `/W4 /WX`.

The next integration step is an opt-in device/MIDI adapter that observes copied
input in output-suppressed shadow mode. It must not install a service, connect a
live graph, emit MIDI, or replace the protected services.
