# Windows Reference Machine: Beanstar

Status: selected; physical portable protocol, generation-adoption, Windows
named-pipe IPC, and JSON dependency evidence pass. Audio, MIDI, plugin-host,
and complete performance certification remain pending.

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

## Next Evidence

The first native bundle passed on 2026-08-10. Its manifest matched locally and
on `beanstar`; the protocol fixture, generation adoption, and 1,000 named-pipe
round trips passed; and cleanup left no bundle directory, process, or pipe. See
[windows-beanstar-m0.json](benchmarks/windows-beanstar-m0.json).

The pinned `json-c` 0.18 bundle also passed on 2026-08-10. The exact Linux
fixture, static library, license, probe, and unlinked baseline hashes matched on
`beanstar`; 10,000 parses completed with zero failures; and cleanup left no
bundle directory or process. See
[json-c-windows.json](benchmarks/json-c-windows.json).

Remaining reference evidence:

1. Inventory and select the Windows audio, MIDI, plugin-host, and service
   backends before any hardware certification claim.
2. Add short-process and daemon resource measurement that reliably samples
   Windows working set and idle CPU.
