# Music Rig IPC Protocol

Status: protocol v2 freezes the complete operation inventory. The portable
runtime implements output-suppressed global and device transactions with
generation guards and durable state. Linux `SOCK_SEQPACKET` now has an explicit
authenticated daemon/client host; Windows transport remains a separate adapter.

## Versioning

Frames are fixed-size little-endian byte sequences. C structure layout is never
sent directly, so compiler padding and host byte order cannot change the wire
format. Decoders require the exact frame size, version, reserved zeros, bounded
counts, known flags, and schema-safe identifiers.

Protocol v1 was the 32-byte request and 40-byte status-only Milestone 0 spike.
Protocol v2 is intentionally incompatible because it freezes operation
arguments and bounded response data before any production endpoint exists.

## Request Frame

Every v2 request is exactly 176 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic `MRIG` |
| 4 | 4 | Protocol version, `2` |
| 8 | 4 | Operation |
| 12 | 4 | Request flags |
| 16 | 8 | Nonzero request ID |
| 24 | 8 | Expected generation, or zero for no precondition |
| 32 | 65 | NUL-terminated device slot, when required |
| 97 | 65 | NUL-terminated profile ID, when required |
| 162 | 14 | Reserved, zero |

Request flag `0x1` means dry-run. No other request flag is accepted.

The complete operation inventory is:

| ID | Operation | Arguments | Current portable behavior |
| ---: | --- | --- | --- |
| 1 | `status` | none | Read-only |
| 2 | `list-profiles` | optional device slot | Read-only |
| 3 | `prepare-global` | profile | Dry-run only |
| 4 | `prepare-device` | device slot and profile | Dry-run only |
| 5 | `switch-global` | profile | Dry-run or runtime commit |
| 6 | `switch-device` | device slot and profile | Dry-run or runtime commit |
| 7 | `reset-device-override` | device slot | Dry-run or runtime commit |
| 8 | `reload-compiled-definition` | none | Dry-run validation only |
| 9 | `validate-active` | none | Read-only |

A dry-run can inspect the active immutable definition plus a caller-owned
catalogue of at most 16 explicitly prepared definitions. Every candidate must
pass full compiled-definition validation and expose the same stable device-slot
port catalogue as the active definition. Candidate profile rows are available
but not active. A dry-run never publishes a generation, persists state, loads a
resource, contacts a device, or changes a graph.

The non-mutating table dispatcher still rejects every commit-capable request.
The portable runtime transaction accepts non-dry-run global and device requests
in output-suppressed mode and through an explicit output adapter in
output-enabled mode. It checks the expected generation,
reuses only validated control-only table images, reserves bounded rollback
storage, publishes a monotonically increasing immutable generation, and
atomically persists the active Rig Profile and device override set. A persistence
failure republishes the previous table image with a newer generation and reports
rollback success or failure explicitly.

## Response Frame

Every v2 response is exactly 2,592 bytes. The fixed inventory holds all 16
profiles allowed by compiled-table ABI v1.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic `MRIG` |
| 4 | 4 | Protocol version, `2` |
| 8 | 4 | Operation |
| 12 | 4 | Result code |
| 16 | 4 | Response flags |
| 20 | 4 | Aggregate or selected readiness |
| 24 | 8 | Request ID |
| 32 | 8 | Previous generation |
| 40 | 8 | Resulting generation |
| 48 | 8 | Control-plane duration in nanoseconds |
| 56 | 8 | Effective adoption timestamp, zero when not applicable |
| 64 | 4 | Rollback status |
| 68 | 4 | Warning flags |
| 72 | 4 | Profile row count, at most 16 |
| 76 | 4 | Output mode, `0` suppressed or `1` enabled |
| 80 | 65 | Active Rig Profile |
| 145 | 65 | Selected device slot, when applicable |
| 210 | 65 | Selected target profile, when applicable |
| 275 | 13 | Reserved, zero |
| 288 | 2,304 | Sixteen fixed 144-byte profile rows |

Each profile row contains a 65-byte device slot, a 65-byte profile ID, two
reserved zero bytes, 32-bit readiness, 32-bit profile flags, and four reserved
zero bytes. Unused rows must be entirely zero.

Response flags identify output suppression (`0x1`), dry-run (`0x2`), a valid
plan or active definition (`0x4`), and an empty graph delta (`0x8`). Profile
flags identify active (`0x1`) and override (`0x2`) rows. Readiness values are
not evaluated (`0`), control-only (`1`), prepared (`2`), and cold (`3`). Warning
flags currently identify cold loading (`0x1`) and unavailable bindings (`0x2`).
Rollback is not required (`0`), succeeded (`1`), or failed (`2`).

Result codes remain stable:

| Code | Meaning |
| ---: | --- |
| 0 | Success |
| 1 | Unsupported operation or mode |
| 2 | Invalid argument or structured request |
| 3 | Invalid runtime lifecycle state |
| 4 | Platform adapter failure |
| 5 | Expected-generation conflict |
| 6 | Requested profile or storage object not found |
| 7 | Invalid or corrupt structured data |
| 8 | Caller-owned buffer is too small |

Read-only and dry-run responses keep previous and resulting generations
equal. A successful global commit reports the generation observed before the
transaction and the newly published generation. A failed persistence step
reports the rollback generation when rollback publication succeeds. A stale
nonzero expected generation returns code 5 before publication.
For a global commit, control-plane duration stops at immutable generation
publication; the following persistence and any rollback work are not folded into
the commit-latency measurement.

## CLI Contract

The portable client parser and renderer support:

```text
music-rig status [--json] [--expected-generation ID]
music-rig profiles list [--device SLOT] [--json]
music-rig validate [--json]
music-rig prepare --global PROFILE --dry-run [--json]
music-rig prepare --device SLOT --profile PROFILE --dry-run [--json]
music-rig switch --global PROFILE [--dry-run] [--json]
music-rig switch --device SLOT --profile PROFILE [--dry-run] [--json]
music-rig reset --device SLOT [--dry-run] [--json]
```

Human and versioned JSON rendering use the same decoded response. The parser
accepts commit-capable global/device switches and reset, while still requiring
`--dry-run` for prepare commands. It rejects conflicting
scopes, duplicate options, invalid identifiers, and numeric overflow. The
Linux builds connect to the resolved per-user control socket. If the daemon is
absent, recognized commands return adapter failure 4 without sending a request.

## Mock Transports

The Linux test uses an unnamed local `SOCK_SEQPACKET` pair. The Windows test
uses a unique local message-mode named pipe with a current-user-only ACL and
remote clients rejected. Both exchange shared v2 golden prefixes and run 1,000
mixed status, filtered-list, validation, and device dry-run requests through
the real immutable-table dispatcher. They create no filesystem socket, default
pipe, service, runtime state, device connection, or musical output.

The Linux production adapter uses a filesystem `SOCK_SEQPACKET` endpoint,
restricts the socket to the current user, validates `SO_PEERCRED`, and rejects
truncated or oversized frames. Malformed and unauthorized peers are closed so
they cannot terminate or hold the daemon's serialized client slot. Client and
response I/O has a bounded two-second timeout. A second daemon refuses an active
socket rather than unlinking it. The explicit daemon run command
loads a fingerprint-verified compiled definition and serves the runtime
dispatcher. The Windows production adapter requires the equivalent lifecycle
and timeout handling described in
[ADR 0004](../../features/0001.0000.0000.0000-configurable-performance-rig/architecture-decisions/0004-windows-local-ipc-named-pipes.md).
