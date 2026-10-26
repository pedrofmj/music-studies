# Windows Arturia Shared-SF2 Host

Status: code-only WinMM adapter, shared route contract, Insync SoundFont root
resolver, portable Carla host boundary, and Windows Carla 2.5.10 DLL adapter are
present. The official Carla 2.5.10 x64 archive checksum and DLL Host API exports
are verified. Carla engine startup, project playback, audio/MIDI integration,
and physical certification remain unverified.

## Scope

The branch adds a portable Arturia session core, an injected Carla host boundary,
and a native WinMM MIDI adapter around the portable Arturia control state and
shared-SF2 route plan. Device identity is the WinMM device-interface path, not a
persistent numeric device index. MIDI callbacks timestamp and enqueue into
bounded rings; the input worker can notify an observer which only enqueues into
the Carla host's bounded MIDI queue. The owner thread drains that queue and makes
all Carla calls. The fake WinMM seam tests identity reorder, ambiguity, bounded
queues, control-state replay, and shutdown without opening a device.

The session core accepts an injected engine interface, so the same profile
switch transaction can drive Carla or another SF2 host. It applies cached
fader/knob values, keeps master volume/mute separate, routes notes and common
controllers to active layers, and rolls back failed activation or control
updates. The Windows Carla adapter dynamically loads the required Carla Host API
exports and does not link against Carla headers or import libraries. Its open
operation accepts the DLL/project paths, driver and engine client names, exact
Windows MIDI Services Basic Loopback input client/port names, SF2 plugin
instance names, the shared-SF2 routing plan, and a required raw-MIDI output
callback. It sets patchbay process mode before engine startup, loads the project
after startup, idles while waiting for patchbay enumeration, and validates exact
plugin instance names, SF2 type, and the runtime `Reverb Level` parameter name,
index, and 0..1 range. Carla connects only the named Basic Loopback input to each
plugin's MIDI event input and deactivates all catalog patches before selecting
the initial profile. Callback records are bounded and synchronized; callback
overflow, oversized names, ambiguous matches, or failed connections abort
opening rather than accepting partial routing.

The upstream Carla standalone C API requires engine initialization before
project loading and regular `carla_engine_idle()` calls from the host thread.
The portable Carla boundary now requires and exposes that idle operation; its
caller must serialize idle, profile, MIDI, queue-drain, and close calls on one
host thread and stop/join the WinMM input worker before closing the Carla host.
The WinMM input worker may enqueue complete short MIDI messages but must not
call Carla. The Carla owner thread drains them through the portable session, so
non-channel-1 pads and unsupported messages are filtered before routing. Notes,
modulation CC1, sustain CC64, and pitch bend are all enqueued in FIFO order to the
WinMM Basic Loopback output. Carla's loopback patchbay connections deliver that
filtered stream to active SF2 patches; no second path sends notes directly.
Fader, knob, master-volume, and mute controls remain direct Carla operations.
Carla has no generic raw MIDI message API, which is why approved raw events use
the WinMM loopback instead.

The Windows-only `music_rig_win32_carla_runtime` library coordinates these
adapters. Its open operation accepts injected WinMM operations/context, an
endpoint table pairing the physical Arturia input path with WinMM output paths,
the Carla DLL/project and patch-plan options (including exact Basic Loopback
input names), and the slot that selects the dedicated Basic Loopback output. It
installs an observer that only enqueues complete MIDI into Carla's bounded queue,
opens Carla on the calling thread with raw MIDI output sent through
`music_rig_winmm_midi_send`, then starts WinMM. Call `idle` or `pump` regularly
on that same thread; pumping idles Carla and drains queued MIDI in FIFO order.
Profile selection and close are also owner-thread operations. Close stops and
joins WinMM before Carla is closed, and retains a retryable closing runtime if
WinMM resources could not be released. Carla is configured only with the named
loopback input, never the physical Arturia input.
Patch faders are scaled to 0..1 and multiplied by master volume; master mute
sets all plugin gains to zero and unmute restores the tracked fader/master
values. The adapter enables and verifies Carla's raw control-change and
pitch-bend options for each SF2 plugin. It does not enable raw-note suppression:
the loopback carries only the notes and controls approved by the portable
session. No physical Arturia input is connected to Carla. The API baseline is
Carla 2.5.10
([release tag `3834c7f`](https://github.com/falkTX/Carla/tree/v2.5.10)), based on
its [`CarlaHost.h`](https://github.com/falkTX/Carla/blob/v2.5.10/source/backend/CarlaHost.h).
The adapter targets the v2.5.10 C ABI at tag commit `3834c7f`.

The shared catalog uses relative SoundFont paths from
`arturia-genre-patches.json`. `packaging/package_arturia_sf2_pack.py` inventories
the active shared-SF2 paths from a build-time source root and can stage one
content-addressed pack. Staging requires hash-matched redistribution approval,
license text, attribution, source URL, and reviewer metadata for every source
file. Existing external-asset records do not establish redistribution rights;
no Galaxy Sound Bank files are copied into the repository or published by this
work. The Windows Insync path resolver is an offline-tested utility and is not
currently called by the Carla runtime. The offline project materializer can
rewrite a caller-provided Carla project to reference the staged pack while
preserving its audio patchbay connections; application installation and runtime
asset-root discovery remain unimplemented.

## Current Boundary

The Windows Carla adapter is connected to an explicit, opt-in
`music-rigd run-win32-carla` lifecycle on Windows. The ABI probe confirms that
the `libcarla_standalone2.dll` from the official package loads and exports the
required symbols, but the command has not initialized Carla's engine or loaded
a project against the real runtime. Audio output, ASIO/WASAPI configuration,
dynamic plugin/resource dependencies, and Windows MIDI device binding are not
certified. The fake-DLL test does not establish a playable Windows rig.

The command is never started by `music-rigd`'s default behavior. It requires
caller-supplied Carla DLL and `.carxp` project paths, driver/client names, exact
Basic Loopback input client/port names, patch names and indexes, active layers,
master volume, and WinMM interface paths. It configures one WinMM endpoint slot,
`arturia-main`: the supplied physical controller is its input and the dedicated
Windows MIDI Services Basic Loopback output is its output. Carla's MIDI client
and port must identify the corresponding Basic Loopback input. Only explicitly
running this command starts Carla and opens real audio/MIDI endpoints; help and
malformed-option checks start neither. The command selects the supplied initial
profile and can accept repeatable caller-configured
`--pad-profile CODE:PATCH_INDEXES:ACTIVE_LAYERS` entries. Code 1..16 follows the
same ordering as Android `ArturiaPadModes`: Bank A pads 1..8, then Bank B pads
1..8. Code 2 (Bank A Pad 2, Synth Programmer) is intentionally unsupported by
this shared-SF2 runtime. Each entry has nine zero-based catalog indexes and 1..9
unique active layers; its plan is copied and validated before WinMM starts.
Channel-10 Arturia pad presses enqueue profile events alongside MIDI in one FIFO,
and only the Carla owner-thread pump applies them. Pad releases and unsupported
or unmapped management pads are suppressed rather than routed as performance
MIDI. Without pad-profile options, the initial-profile behavior is unchanged.
`benchmarks/generate-win32-arturia-command.py` derives the command and an
optional patch manifest from the shared genre/routing JSON. The Carla project
must contain an instance for each manifest name. The current catalog produces 22
instances for 15 distinct SF2 paths because aliased patch IDs need independent
layer controls; the duplicated SF2 memory cost has not been measured. The pack
stager stores identical file contents once, and the project materializer
translates the pack's relative asset paths into absolute paths in a generated
project copy. The caller-provided project must already contain all 22 plugin
instances and its required audio routing.
The parser rejects malformed, duplicate, missing, out-of-range, or invalid
UTF-8 endpoint arguments before initializing WinMM or Carla.

Deployment requires Windows MIDI Services in **Full mode** and a configured,
dedicated Basic Loopback endpoint pair. WinMM opens the physical Arturia input
and the Basic Loopback output; Carla's exact external MIDI client/port options
must identify the matching Basic Loopback input. The coordinator does not create
or enumerate that endpoint: deployment must still provide the dedicated pair and
its exact Carla patchbay names. Carla never opens the physical Arturia input, so
it is not opened twice by Carla. Do not substitute the
physical Arturia endpoint for either Carla patchbay name. Ricestar's read-only
registry state has `midi1=wdmaud2.drv` and no `UseLegacyMidi` override (which
defaults to Full mode). `MidiSrv` is installed and demand-started, but the actual
configured loopback/controller/driver combination has not been tested. No MIDI
service or device was started during that check. Legacy or Hybrid mode is not
supported for this deployment and must fail closed if the exact loopback source
is unavailable. See
[Windows MIDI API compatibility](https://microsoft.github.io/MIDI/kb/api-back-compat/)
and [API modes](https://microsoft.github.io/MIDI/kb/how-to-change-api-mode/).

## Offline Checks

`music_rig_winmm_midi_offline` compiles the platform-independent adapter with
fake WinMM operations. It covers shadow and observer-only delivery, observer
failure accounting, queueing, and shutdown. On Windows, the
`music_rig_winmm_midi` target additionally compiles the native WinMM wrapper and
links `Winmm.dll`. Neither test enumerates or opens physical MIDI devices or
audio hardware.

The Windows-only `music_rig_win32_soundfont_paths` test uses temporary
directories to cover direct and Insync root lookup, path traversal rejection,
and small output buffers. It does not access SoundFont or MIDI hardware.

The Windows-only `music_rig_win32_carla_host_offline` test loads a fake Carla DLL
to exercise symbol resolution, startup order, patchbay enumeration and routing,
profile/MIDI calls, filtered FIFO queue behavior and overflow, single-send raw
CC/pitch loopback routing, and cleanup without physical devices. It does not
verify the official Carla DLL ABI or runtime bundle.

The Windows-only `music_rig_win32_carla_runtime_offline` test combines that fake
Carla DLL with fake WinMM operations. It injects input through the WinMM callback
ring, waits for deterministic observer delivery, pumps Carla, checks filtered
FIFO loopback output, verifies WinMM shutdown precedes Carla close, and exercises
WinMM-start failure cleanup. It starts no physical MIDI/audio devices or Carla
engine.

`music_rig_win32_carla_abi_probe.exe <path-to-libcarla_standalone2.dll>` loads a
real Carla DLL and resolves the required exports without starting the engine or
opening MIDI/audio devices. The inspected `Carla-2.5.10-win64.zip` release asset
matches SHA-256
`56264cd860b3e6e7b29221fb1e46d7d8a288fb001500f830bcd739ec7b115650`.
On Ricestar, the probe successfully loaded the extracted release DLL and
resolved all required exports. The Windows libraries and six focused CTests
also built and passed with MSVC 19.51/CMake 4.2.3. This verified the fake Carla
ABI/patchbay/runtime contract and static DLL loading only; the full
Python-driven suite, Carla engine initialization, project playback, and
physical-device behavior were not run.

The portable `music_rig_arturia_session_core` test exercises startup defaults,
Organ/mode profile switching, fader/knob replay, master encoder handling, note,
sustain, modulation and pitch-bend routing, and failure rollback through an
injected fake engine.

The `music_rig_arturia_carla_host` test also exercises the portable Carla
operation seam, including idle, MIDI connection failure, and the
connect/deactivate-before-initial-activation ordering. Neither test loads a
Carla DLL or verifies the Windows ABI, callback enumeration, or patchbay routing
against Carla itself.
