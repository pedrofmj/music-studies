# Windows Arturia Shared-SF2 Host

Status: code-only WinMM adapter, shared route contract, and Insync SoundFont
root resolver are present. A Carla audio-host bridge and physical certification
remain pending.

## Scope

The branch adds a portable Arturia session core and native WinMM MIDI adapter
around the portable Arturia control state and shared-SF2 route plan. Device identity is the WinMM device-interface
path, not a persistent numeric device index. MIDI callbacks only timestamp and
enqueue into bounded rings; worker threads drain input, route behavior output,
and send queued output. The fake WinMM seam tests identity reorder, ambiguity,
bounded queues, control-state replay, and shutdown without opening a device.

The session core accepts an injected engine interface, so the same profile
switch transaction can drive Carla or another SF2 host. It applies cached
fader/knob values, keeps master volume/mute separate, routes notes and common
controllers to active layers, and rolls back failed activation or control
updates. The Windows branch does not yet provide the concrete Carla engine
adapter.

The shared catalog uses relative SoundFont paths from
`arturia-genre-patches.json`. The Windows path adapter resolves the user's
Known-Folder profile, checks the direct `Flash/PED/MIDI/Pack de Timbres/Library`
layout, then searches one account level below `Insync/`. It rejects absolute or
parent-traversal catalog paths. No SF2 files are copied into the repository.

## Current Boundary

This adapter is not yet connected to a production `music-rigd` lifecycle or a
Carla shared-SF2 host. In particular, no Carla backend is launched, no ASIO or
WASAPI output is opened, and no Windows device binding is certified. The code
therefore proves the MIDI adapter contract only, not a playable Windows rig.

## Offline Checks

`music_rig_winmm_midi_offline` compiles the platform-independent adapter with
fake WinMM operations. On Windows, the `music_rig_winmm_midi` target additionally
compiles the native WinMM wrapper and links `Winmm.dll`. Neither test accesses
physical MIDI or audio hardware.

The portable `music_rig_arturia_session_core` test exercises startup defaults,
Organ/mode profile switching, fader/knob replay, master encoder handling, note,
sustain, modulation and pitch-bend routing, and failure rollback through an
injected fake engine.
