# Music Rig IPC Protocol

Status: Milestone 0 framing contract with Linux `SOCK_SEQPACKET` and Windows
named pipes selected. The first Milestone 3 runtime dispatcher consumes decoded
status requests through a mock control adapter. This is not yet the complete
runtime command or transport contract.

## Portable Frame

Version 1 uses fixed-size little-endian frames. C structure layout is never sent
directly, so compiler padding and host byte order cannot change the wire format.

Request frame, 32 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic `MRIG` |
| 4 | 4 | Protocol version |
| 8 | 4 | Operation |
| 12 | 4 | Reserved, zero |
| 16 | 8 | Request ID |
| 24 | 8 | Expected generation |

Response frame, 40 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic `MRIG` |
| 4 | 4 | Protocol version |
| 8 | 4 | Result code |
| 12 | 4 | Reserved, zero |
| 16 | 8 | Request ID |
| 24 | 8 | Previous generation |
| 32 | 8 | Resulting generation |

The runtime implements only the `status` operation. Its result codes are:

| Code | Meaning |
| ---: | --- |
| 0 | Success |
| 1 | Unsupported operation or mode |
| 2 | Invalid argument or structured request |
| 3 | Invalid runtime lifecycle state |
| 4 | Platform adapter failure |
| 5 | Expected-generation conflict |

For status, a zero expected generation means no precondition. A nonzero value
must match the current published generation or the response returns code 5.
Additional operations and payloads are added only after their fields and size
limits are frozen here.

## Linux Transport

Linux uses local Unix `SOCK_SEQPACKET`. It preserves one frame per message and
rejects partial framing assumptions. The Milestone 0 test uses `socketpair`, so
it creates no filesystem endpoint, daemon, service, or installed state.

Before a real daemon is introduced, the adapter must add a runtime-directory
socket path, peer credential checks, permissions, bounded timeouts, and explicit
oversized-message rejection.

## Windows Transport

The selected Windows transport is a local named pipe in message mode with an
explicit current-user-only ACL and remote clients rejected. It can preserve the
request/response message boundary while the portable encoder remains unchanged.
The native test sends the shared request and response golden frames before
running 1,000 measured round trips. It passes in hosted MSVC CI and from a
hash-verified artifact on `beanstar`; the accepted transport decision and
production requirements are recorded in
[ADR 0004](../../features/0001.0000.0000.0000-configurable-performance-rig/architecture-decisions/0004-windows-local-ipc-named-pipes.md).
