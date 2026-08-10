# ADR 0004: Windows Local IPC Uses Named Pipes

Status: Accepted for the Windows control plane on 2026-08-10.

## Context

The portable core defines fixed-size little-endian request and response frames.
Linux carries one frame per message over local `SOCK_SEQPACKET`. Windows needs
an equivalent local transport without exposing a network listener, changing the
portable encoder, or adding work to audio or MIDI callbacks.

Windows named pipes support message-type writes and message-read handles. The
Windows API also accepts an explicit security descriptor and can reject remote
clients:

- [Named Pipe Type, Read, and Wait Modes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-type-read-and-wait-modes)
- [Named Pipe Security and Access Rights](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights)
- [`CreateNamedPipe`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea)

## Decision

Use local Windows named pipes as the Windows CLI-to-daemon control transport.
The Windows adapter uses:

- `PIPE_TYPE_MESSAGE` and `PIPE_READMODE_MESSAGE` on the server;
- message-read mode explicitly set on each client handle;
- `PIPE_REJECT_REMOTE_CLIENTS`;
- an explicit DACL instead of the Windows default descriptor;
- `FILE_FLAG_FIRST_PIPE_INSTANCE` on the first server instance;
- fixed protocol-frame buffers and exact-length validation; and
- `QueryPerformanceCounter` for transport timing evidence.

This is a control-plane adapter only. Audio and MIDI callbacks consume already
published immutable generations and never open, read, write, wait on, or parse
named-pipe data.

The spike grants access only to the current user. Production ACL construction
must use the daemon identity and authorized interactive-user or logon SID. It
must grant the minimum individual rights required by the client. Microsoft
documents that generic write access includes `FILE_CREATE_PIPE_INSTANCE`, so a
service-owned pipe must not grant broad generic write access when narrower
rights satisfy the client.

Blocking calls are acceptable in the isolated CLI/control threads used by the
spike. The daemon adapter must add bounded connection and I/O cancellation
before runtime integration.

## Evidence

Commit `4e9a4546057872ed291ae79c394f83913bdb9bd5` passed all ten MSVC tests in
[hosted run 31399835507](https://github.com/pedrofmj/music-studies/actions/runs/31399835507).
The same hash-verified artifact then ran on `beanstar` without a repository
checkout:

- portable request and response golden frames: passed;
- current-user-only message-mode pipe: passed;
- 1,000 round trips with zero failures;
- p50: 8,000 ns;
- p95: 12,000 ns;
- p99: 17,400 ns;
- maximum: 62,100 ns; and
- post-run test processes, pipes, and temporary files: zero.

The complete machine, artifact, hash, run, timing, scope, and cleanup record is
[windows-beanstar-m0.json](../../../tools/music-rig/benchmarks/windows-beanstar-m0.json).

## Consequences

- Windows preserves the same protocol message boundary as Linux without
  leaking a Windows handle or header into the portable core.
- The runtime needs a small Windows-only pipe, ACL, timeout, and peer-validation
  adapter.
- The pipe path and authorized identities become platform binding and service
  lifecycle concerns, not musical profile fields.
- The physical result closes transport selection, not daemon resource, service,
  MIDI, audio, plugin-host, or complete profile-switch certification.

## Alternatives

- Windows Unix-domain sockets remain viable for byte streams but do not provide
  the selected message-mode contract and would require separate framing logic.
- Loopback TCP adds a network endpoint and firewall surface for a machine-local
  control path.
- Byte-mode named pipes discard the message boundary already provided by both
  selected platform transports.

## Revisit Conditions

Revisit this decision if the selected Windows service identity cannot be
expressed with a least-privilege DACL, bounded cancellation cannot meet runtime
shutdown requirements, or later daemon measurements fail the accepted latency
or resource ceilings.
