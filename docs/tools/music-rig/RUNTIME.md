# Portable Runtime Control Loop

The first Milestone 3 runtime slice is a platform-neutral, output-suppressed
control dispatcher. It establishes the state, metrics, generation, and adapter
contracts that future Linux and Windows daemon hosts use. It does not load a
compiled definition from disk, create an IPC endpoint, persist state, bind a
device, inspect or change a graph, contact a plugin host, or control a service.

## Ownership And Storage

`music_rig_runtime` and every immutable `music_rig_generation` use caller-owned
storage. Initialization copies the 32-byte compiled-definition fingerprint and
the versioned adapter table into the runtime; it performs no allocation.

One control thread owns runtime initialization, `music_rig_runtime_run`, state,
and metrics. Platform callbacks execute synchronously on that thread. A future
real-time callback reads only the separately published immutable generation
pointer through the existing lock-free generation slot. State and metric
accessors return live read-only views and are not cross-thread snapshots.

The runtime rejects `MUSIC_RIG_OUTPUT_ENABLED`. Only
`MUSIC_RIG_OUTPUT_SUPPRESSED` can initialize until a later milestone adds and
certifies output adapters through an explicit promotion path.

## Lifecycle

The version 1 lifecycle is:

```text
uninitialized -> initialized -> running -> stopped
                                  |
                                  +-------> failed
```

Initialization requires a nonzero generation, an exact 32-byte definition
fingerprint, output-suppressed mode, and adapter ABI version 1. `run` may be
called exactly once. A normal stop closes the control adapter and records the
monotonic stop time. Start, poll, wait, response, or stop callback failures move
the runtime to `failed` and increment the saturating adapter-failure counter.

The control adapter's `start` callback must leave itself closed when it returns
failure. After a successful start, the runtime calls `stop` exactly once even
when a later callback fails.

## Platform Interfaces

The initial `music_rig_platform_interfaces` table contains two separable
adapters:

| Adapter | Responsibility |
| --- | --- |
| Clock | Return monotonic nanoseconds for lifecycle timestamps |
| Control | Start, poll one decoded request, wait without polling, send one response, and stop |

`poll` returns one of four bounded outcomes:

- `request`: the supplied structure contains one already decoded protocol
  request;
- `idle`: no request is ready, so the runtime invokes the adapter's blocking or
  event-driven `wait` callback;
- `stop`: complete a normal shutdown; or
- `error`: fail the runtime and close the started adapter.

The interface contains no platform handles or backend names. IPC framing,
authentication, paths, device I/O, graphs, plugin hosts, services, and
diagnostics remain separate adapters or later extensions.

## State And Metrics

Versioned state records lifecycle, published generation ID, raw definition
fingerprint, output mode, and monotonic start/stop times. Publishing a new
caller-owned generation supports an optional expected-generation precondition.
Stale preconditions and non-increasing generation IDs fail without changing
state.

All metrics are unsigned 64-bit saturating counters:

- loop iterations, idle polls, and control waits;
- control, status, invalid, and response counts;
- generation publications and conflicts; and
- adapter failures.

Status dispatch returns the current generation. A nonzero stale expected
generation produces result code 5 while keeping the previous and resulting
generation equal. Invalid structured requests produce result code 2.

## Executable Boundary

`music-rigd` is built on Linux and Windows but is deliberately inert. It
supports only `--version` and `--help`; invoking it without a command exits with
code 2. It has no configuration, definition-loading, transport, installation,
service, or default-start path.

CTest exercises successful idle/request/stop sequencing, status and stale
generation responses, invalid requests, publication conflicts, all adapter
failure stages, invalid lifecycle/configuration, ABI rejection, daemon
inertness, and a source audit that rejects allocation and C thread-lock calls
from the runtime dispatcher. Existing portability tests continue to reject
platform headers and backend identifiers from the complete core tree.

## Next Runtime Slice

The next slice freezes the complete read-only and dry-run IPC/CLI contract,
loads one checked compiled definition into caller-owned runtime tables, and
adds mock Linux/Windows control transports. It remains output-suppressed and
does not replace the protected single-rig deployment.
