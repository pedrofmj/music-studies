# ADR 0006: Windows Platform Adapter Baseline

Status: Accepted for implementation on 2026-08-10. Physical audio, MIDI,
plugin-host, lifecycle, and full-profile certification remain pending.

## Context

Windows is the required second platform. The authored Rig and Device Profiles,
portable compiler, state model, CLI, and protocol cannot contain Windows API,
driver, path, or plugin identifiers. Milestone 0 therefore needs a concrete
Windows adapter baseline before Linux implementation choices shape those
portable contracts.

The selected `beanstar` reference machine currently has Windows MIDI Services
and its WinMM compatibility mappings, but no registered ASIO driver, Carla
package-registration evidence, or matching Windows MIDI SDK/tools package
entry. That is enough to select implementation boundaries, not to claim a
playable Windows rig.

The choices below favor a small resident control process, event-driven waits,
no network-facing control listener, and no audio or JSON work in MIDI/audio
callbacks.

## Decision Summary

| Adapter | Selected baseline | Certification state |
| --- | --- | --- |
| IPC | Current-user local message-mode named pipes from [ADR 0004](0004-windows-local-ipc-named-pipes.md) | Hosted and physical synthetic proof passed |
| Clock | `QueryPerformanceCounter` with one cached `QueryPerformanceFrequency` | Physical timing spikes passed |
| Control wakeup | `WaitOnAddress` and `WakeByAddressSingle` around atomic state | Selected; runtime load proof pending |
| Paths and state | `SHGetKnownFolderPath`, per-user local directories, UTF-16 Win32 paths, and same-directory replacement | Selected; adapter tests pending |
| Lifecycle | One non-elevated per-user Task Scheduler logon task | Selected; install/rollback proof pending |
| MIDI | Built-in WinMM MIDI 1.0 API, using device-interface identity where available | Selected; device and load proof pending |
| Direct MIDI Services SDK | Deferred until the application SDK is a signed production dependency | Upstream remains prerelease |
| Audio | Plugin-host-owned vendor ASIO first; low-period WASAPI is the measured fallback | No ASIO driver is currently registered |
| Plugin host and graph | Out-of-process 64-bit Carla, native VST3, internal Patchbay mode | No registered package evidence; host and plugins are uncertified |

## Clock And Thread Boundary

Use `QueryPerformanceCounter` for monotonic interval and event timestamps. Cache
the frequency at process initialization and normalize to nanoseconds with
overflow-safe integer arithmetic. UTC is metadata only and never the latency
clock. Microsoft documents QPC as the native high-resolution, monotonic interval
clock:

- [Acquiring high-resolution time stamps](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)

Use C17 atomics for immutable-generation publication. A control or MIDI worker
may wait on an atomic sequence with `WaitOnAddress`; the producer changes the
sequence and calls `WakeByAddressSingle`. This avoids polling and a permanent
timer tick:

- [`WaitOnAddress`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress)
- [`WakeByAddressSingle`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-wakebyaddresssingle)

Do not raise the whole daemon process priority and do not call `timeBeginPeriod`
globally. Audio-thread priority remains owned by the selected audio driver and
plugin host. Any later thread-priority change requires before/after CPU,
wakeup, dropout, and deadline evidence.

## Path And State Boundary

Resolve paths with `SHGetKnownFolderPath`, never by parsing environment-variable
strings:

- executable root: `FOLDERID_UserProgramFiles/MusicRig`;
- configuration: `FOLDERID_LocalAppData/MusicRig/Config`;
- compiled cache: `FOLDERID_LocalAppData/MusicRig/Cache`;
- persistent active and device state: `FOLDERID_LocalAppData/MusicRig/State`;
  and
- logs and diagnostic evidence: `FOLDERID_LocalAppData/MusicRig/Logs`.

Microsoft recommends the Known Folder API for new desktop code:

- [Known Folders](https://learn.microsoft.com/en-us/windows/win32/shell/known-folders)

Authored definitions stay in Git or another explicitly selected source tree;
they are not copied into operational state. Machine-local bindings do not roam
silently between computers. Cross-platform movement uses the versioned
export/import contract.

Every state write creates and flushes a temporary file beside its destination,
then uses `ReplaceFileW` for an existing destination or same-volume
`MoveFileExW` for first creation. Status reports resolved paths and security.
The per-user root must reject broad write access and preserve its DACL during
replacement:

- [`ReplaceFileW`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)

## Lifecycle Boundary

Run `music-rigd` in the logged-on user's session through one Task Scheduler
logon task. The task is non-elevated, bound to the owning user, configured to
ignore a second instance, allowed on battery power, and given no execution time
limit. Production registration is enabled only during explicit promotion.

The CLI requests graceful shutdown through the authenticated named pipe. Forced
Task Scheduler termination is recovery only. Prepared plugin-host processes
are supervised by the daemon and must be placed in a Windows Job Object so a
failed daemon cannot leave an owned experimental host behind.

This design deliberately rejects a LocalSystem or interactive Windows Service.
Windows services run in noninteractive session 0, while plugin hosts, device
drivers, and optional plugin UIs belong to the selected user's session:

- [Starting an executable when a user logs on](https://learn.microsoft.com/en-us/windows/win32/taskschd/starting-an-executable-when-a-user-logs-on)
- [Task Scheduler multiple-instance policy](https://learn.microsoft.com/en-us/windows/win32/taskschd/tasksettings-multipleinstances)
- [Interactive services and session 0](https://learn.microsoft.com/en-us/windows/win32/services/interactive-services)

## MIDI Boundary

The first Windows MIDI adapter uses the stable WinMM C ABI from `Winmm.dll`.
It opens MIDI 1.0 ports with `midiInOpen`/`midiOutOpen` and requests each
device-interface path with `DRV_QUERYDEVICEINTERFACESIZE` and
`DRV_QUERYDEVICEINTERFACE`. Numeric WinMM indexes are discovery-time values,
not persistent slot identifiers. A display name is never sufficient to
disambiguate two same-model controllers.

- [`midiInOpen`](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-midiinopen)
- [`DRV_QUERYDEVICEINTERFACE`](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/drv-querydeviceinterface)

The callback performs only bounded work: capture QPC time, normalize the fixed
MIDI 1.0 message, enqueue it in a preallocated single-producer/single-consumer
ring, update overflow/error counters, and wake the worker. It does not call a
multimedia function, allocate, log, parse, switch a profile, or wait. Microsoft
warns that multimedia calls inside `MidiInProc` can deadlock:

- [`MidiInProc`](https://learn.microsoft.com/en-us/previous-versions/dd798460%28v%3Dvs.85%29)

Long messages use preallocated buffers prepared outside the callback. Port
open, close, discovery, reconnect, and capability work run on the control
thread. Output is consumed by a dedicated bounded worker rather than the CLI
thread.

Current Windows 11 routes compatible WinMM and WinRT MIDI 1.0 clients through
Windows MIDI Services where the feature is enabled, retaining the built-in ABI
while gaining service improvements. The direct Windows MIDI Services SDK is
not the first dependency because its current official releases remain
prerelease and its runtime/tools package is separately installed:

- [Windows MIDI Services](https://microsoft.github.io/MIDI/)
- [Windows MIDI Services releases](https://github.com/microsoft/MIDI/releases)
- [Persistent endpoint identifier guidance](https://microsoft.github.io/MIDI/kb/identifiers/)

A later direct SDK adapter is allowed behind the same portable MIDI interface
after a signed production release, repeatable package path, C/C++ boundary,
footprint measurement, device identity test, and fallback behavior pass.

## Audio And Plugin-Host Boundary

`music-rigd` does not open an audio endpoint or process samples. The plugin host
owns the audio driver, processing callback, internal graph, plugins, and sample
memory. This keeps control-only profile switching independent from a Windows
audio engine and reports daemon resources separately from prepared engines.

The full-performance driver order is:

1. the selected audio interface's supported native 64-bit ASIO driver;
2. low-period WASAPI through `IAudioClient3`, if the chosen host exposes it and
   physical measurements meet every latency and stability gate; and
3. WASAPI exclusive mode only as an explicit binding because it seizes the
   endpoint.

DirectSound may be used for offline development but cannot certify a live Rig
Profile. Microsoft documents both low-period WASAPI and third-party ASIO as
low-latency Windows paths:

- [Low-latency audio](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio)
- [WASAPI exclusive-mode streams](https://learn.microsoft.com/en-us/windows/win32/coreaudio/exclusive-mode-streams)

Use the official 64-bit Carla build as the first Windows plugin-host candidate.
The initial binding targets native VST3 plugins and Carla's internal Patchbay
mode. Carla is process-isolated from `music-rigd`; the daemon never loads a
plugin DLL. A Windows host bridge may use Carla's documented backend API for
project, parameter, graph, readiness, and xrun operations, but it is a separate
component with explicit GPL compliance. Carla OSC is diagnostic only unless a
later security proof demonstrates local-only exposure and authorization.

- [Carla application and Windows build](https://kx.studio/Applications%3ACarla)
- [Carla Backend API](https://kx.studio/ns/dev-docs/CarlaBackend/group__CarlaBackendAPI.html)
- [Carla Host API](https://kx.studio/ns/dev-docs/CarlaBackend/group__CarlaHostAPI.html)

High-rate transformed MIDI ingress into the host requires a bounded,
preallocated host-side queue. Per-event CLI or JSON messages are prohibited.
Its exact cross-process transport is a Milestone 7 spike and must prove the
common event-latency, overflow, reconnect, and generation-adoption contracts
before a complete Windows binding can pass.

## Reference Evidence

The read-only `beanstar` capture on 2026-08-10 found:

- `MidiSrv` present, stopped, and configured for manual/on-demand start;
- WinMM compatibility mappings `midi=wdmaud.drv` and
  `midi1=wdmaud2.drv`;
- zero registered 32-bit or 64-bit ASIO drivers;
- no matching Carla or Windows MIDI SDK/tools uninstall entry; and
- Task Scheduler running with automatic startup.

No endpoint was enumerated or opened, no process or service was started, and no
machine value was changed. The raw observation and upstream version snapshot is
[windows-backend-capabilities.json](../../../tools/music-rig/benchmarks/windows-backend-capabilities.json).

## Certification Gates

Selection closes the Milestone 0 design question. It does not close Milestone 7.
The Windows implementation must still prove:

- default and mock-adapter builds with no optional backend dependency;
- deterministic path/state replacement and permission tests;
- scheduled-task install, graceful restart, crash recovery, uninstall, and
  rollback with no orphan process;
- exact slot binding for all five controllers, including same-model ambiguity,
  disconnect, reconnect, and port reorder;
- at least 1,000 MIDI events in each benchmark condition with zero loss and the
  accepted management-trigger and resource ceilings;
- a recorded native ASIO or qualified low-period WASAPI driver, buffer size,
  sample rate, and zero attributable dropout/deadline failure;
- a pinned Carla artifact, plugin inventory, project fingerprint, semantic
  parameter mapping, preparation, failure injection, and rollback; and
- one complete Rig Profile from the same authored definitions used by Linux.

Until those gates pass, Windows reports audio and plugin-host capabilities as
unsupported or unavailable and never silently selects DirectSound or a generic
endpoint.

## Alternatives And Revisit Conditions

- A LocalSystem Windows Service was rejected because the rig is a per-user,
  interactive-session workload.
- Direct Windows MIDI Services integration was deferred, not rejected; revisit
  it when the SDK is production-signed and stable.
- JACK2 on Windows is not the first graph layer because Carla already provides
  an internal patchbay and a separate graph daemon would add lifecycle and
  resource cost. Revisit it only if the internal host boundary cannot satisfy
  required routing or transactional behavior.
- DirectSound is not a low-latency certification fallback.
- Revisit Carla if the exact Windows build cannot provide the required native
  plugin formats, driver, control API, prepared engines, or rollback semantics.
