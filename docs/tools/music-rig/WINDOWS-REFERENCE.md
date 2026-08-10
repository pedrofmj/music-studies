# Windows Reference Machine: Beanstar

Status: selected; physical portable protocol, generation-adoption, Windows
named-pipe IPC, JSON dependency, and Milestone 0 process-resource evidence
pass. The Windows adapter baseline is selected; audio, MIDI, plugin-host,
lifecycle, and complete performance certification remain pending.

## Role

`beanstar` is the physical Windows reference machine for the configurable
performance rig. It complements the hosted Windows Server 2025 build, which
proves compilation and portable contracts but has no music hardware.

Selecting the machine does not claim that every Windows backend is ready. The
current inventory is sufficient for native process, path, clock, IPC, and
resource proofs. Audio, MIDI, ASIO, plugin-host, dropout, and end-to-end profile
switching evidence is accepted only after the required devices and backends are
present and recorded.

## Reference Inventory

The following read-only inventory was captured remotely on
2026-08-10T14:28:54Z:

| Property | Recorded value |
| --- | --- |
| Host name | `BEANSTAR` |
| Manufacturer | Lenovo |
| Product | Legion 5 15IMH05H, machine type `82CF` |
| CPU | Intel Core i7-10750H at 2.60 GHz |
| CPU topology | 6 cores, 12 logical processors |
| Installed memory | 17,083,187,200 bytes, nominally 16 GiB |
| Operating system | Microsoft Windows 11 Pro, 64-bit |
| Display version | 25H2 |
| Version and revision | `10.0.26200.8875` |
| Build lab | `26100.1.amd64fre.ge_release.240331-1435` |

The legacy `ProductName` registry value reports `Windows 10 Pro`; the CIM
operating-system caption reports `Microsoft Windows 11 Pro`. Evidence records
both underlying values when relevant and identifies the system by version,
build, revision, edition, and architecture rather than by the legacy registry
label alone.

Present physical sound devices at capture time:

| Device | Provider | Driver version | State |
| --- | --- | --- | --- |
| Realtek High Definition Audio (SST) | Realtek Semiconductor Corp. | `6.0.8960.1` | OK |
| NVIDIA High Definition Audio | NVIDIA Corporation | `1.3.39.3` | OK |

No 32-bit or 64-bit ASIO driver was registered at capture time. This is an
inventory observation, not a backend decision. External audio and MIDI devices
were not part of this capture and must be inventoried again when their gates
are exercised.

### Backend Capability Snapshot

A second read-only capture at 2026-08-10T18:31:51Z recorded:

| Capability | Recorded value |
| --- | --- |
| Windows MIDI service | `MidiSrv` present, stopped, manual/on-demand start |
| WinMM compatibility | `midi=wdmaud.drv`; `midi1=wdmaud2.drv` |
| ASIO registration | No 32-bit or 64-bit entries |
| Carla | No matching installed-package entry |
| Windows MIDI SDK/tools | No matching installed-package entry |
| Task Scheduler | Running, automatic start |

No audio or MIDI endpoint was enumerated or opened. No service, process other
than the read-only PowerShell query, file, registry value, or task was changed.
The machine-readable snapshot is
[windows-backend-capabilities.json](benchmarks/windows-backend-capabilities.json).
The selected implementation boundary is
[ADR 0006](../../features/0001.0000.0000.0000-configurable-performance-rig/architecture-decisions/0006-windows-platform-adapter-baseline.md).

## Access And Test Boundary

The reference machine is reached over key-authenticated OpenSSH as the local
`beanstar\pedro` account. Port 22 is enabled only through the Windows Private
network-profile firewall rule.

The repository is not cloned, checked out, or opened on `beanstar`. Native
reference tests use a reviewed standalone executable or test bundle produced
from a recorded commit. Each accepted run records the commit, artifact hash,
Windows build, command, result, and cleanup outcome.

Default reference tests must not:

- install or register a service;
- modify audio or MIDI routes, device settings, or default endpoints;
- load a plugin host or alter a plugin project;
- change the existing performance-rig baseline; or
- leave test files, pipes, processes, firewall rules, or startup entries behind.

Any test that needs a real audio or MIDI device is a separately approved live
operation with a preflight, bounded rollback, and post-test validation. Until
then, named-pipe and other platform proofs remain synthetic and output-free.

## Recorded Evidence

The first native bundle passed on 2026-08-10. Its manifest matched locally and
on `beanstar`; the protocol fixture, generation adoption, and 1,000 named-pipe
round trips passed; and cleanup left no bundle directory, process, or pipe. See
[windows-beanstar-m0.json](benchmarks/windows-beanstar-m0.json).

The pinned `json-c` 0.18 bundle also passed on 2026-08-10. The exact Linux
fixture, static library, license, probe, and unlinked baseline hashes matched on
`beanstar`; 10,000 parses completed with zero failures; and cleanup left no
bundle directory or process. See
[json-c-windows.json](benchmarks/json-c-windows.json).

The native resource bundle passed on 2026-08-10. The 60.259-second zero-event
window recorded zero control requests, zero MIDI events, 0 ns of child CPU time
(0.000000% of one core in this observation), a 3,670,016-byte maximum observed
working set, 65 maximum handles, and four maximum threads. The short
`music-rig --version` process was sampled successfully. Both the runner and an
independent post-run query confirmed zero remaining processes and removal of
the temporary directory. See
[windows-resource-beanstar.json](benchmarks/windows-resource-beanstar.json).

Remaining reference evidence:

1. Inventory the five MIDI controllers and selected external audio interface
   with their exact drivers before opening any endpoint.
2. Install and certify the selected ASIO/WASAPI, Carla, MIDI, and per-user
   lifecycle adapters only through separately approved live tests.

These remaining items belong to Windows platform and live-adapter
certification; they do not reopen the completed synthetic Milestone 0 resource
gate.
