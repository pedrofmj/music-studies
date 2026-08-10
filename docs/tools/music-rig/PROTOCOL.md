# Music Rig IPC Protocol

Status: Milestone 0 transport and framing spike. This is not yet the complete
runtime command contract.

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

The spike implements only the `status` operation. Additional operations and
payloads are added only after their fields and size limits are frozen here.

## Linux Transport

Linux uses local Unix `SOCK_SEQPACKET`. It preserves one frame per message and
rejects partial framing assumptions. The Milestone 0 test uses `socketpair`, so
it creates no filesystem endpoint, daemon, service, or installed state.

Before a real daemon is introduced, the adapter must add a runtime-directory
socket path, peer credential checks, permissions, bounded timeouts, and explicit
oversized-message rejection.

## Windows Candidate

The candidate Windows transport is a local named pipe in message mode with an
explicit ACL. It can preserve the request/response message boundary while the
portable encoder remains unchanged. The choice is provisional until the same
golden frames and round-trip tests pass on the selected Windows reference
machine.
