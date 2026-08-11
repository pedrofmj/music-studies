# Portable Runtime Control Loop

The Milestone 3 runtime is a platform-neutral, output-suppressed control
dispatcher. It establishes the definition, qualified persistent state, metrics,
generation, and adapter contracts that future Linux and Windows daemon hosts
use. It does not create an IPC endpoint, implement platform storage paths, bind
a device, inspect or change a graph, contact a plugin host, or control a service.

## Ownership And Storage

`music_rig_runtime`, decoded definition metadata, document workspaces, and every
published immutable `music_rig_generation` use caller-owned storage. Runtime
initialization copies the first generation, 32-byte compiled-definition
fingerprint, and versioned adapter table; it performs no allocation.

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
fingerprint, output-suppressed mode, runtime adapter ABI version 2, and storage
adapter ABI version 1. It reads qualified state before publishing the initial
generation. `run` may be called exactly once. A normal stop closes the control
adapter and records the monotonic stop time. Start, poll, wait, response, stop,
state-read, or state-replace failures remain explicit.

The control adapter's `start` callback must leave itself closed when it returns
failure. After a successful start, the runtime calls `stop` exactly once even
when a later callback fails.

## Platform Interfaces

The `music_rig_platform_interfaces` table contains three separable
adapters:

| Adapter | Responsibility |
| --- | --- |
| Clock | Return monotonic nanoseconds for lifecycle timestamps |
| Control | Start, poll one decoded request, wait without polling, send one response, and stop |
| Storage | Read logical definition/state objects and atomically replace state |

`poll` returns one of four bounded outcomes:

- `request`: the supplied structure contains one already decoded protocol
  request;
- `idle`: no request is ready, so the runtime invokes the adapter's blocking or
  event-driven `wait` callback;
- `stop`: complete a normal shutdown; or
- `error`: fail the runtime and close the started adapter.

Storage callbacks receive logical object identifiers, not paths. A platform
adapter resolves configuration and state locations and must implement
runtime-state replacement atomically. The interface contains no platform
handles or backend names. IPC framing, authentication, actual paths, device I/O,
graphs, plugin hosts, services, and diagnostics remain host implementations or
later extensions.

## Compiled Definition Loading

`music_rig_definition_load` reads a compiled document through the storage
adapter into a caller-owned bounded workspace, invokes a decoder adapter, checks
the portable metadata contract, compares the recorded fingerprint with a
trusted expected fingerprint, and returns validated metadata. A separate call
initializes a caller-owned immutable generation from that metadata.

The optional `json-c` adapter now decodes the checked-in 86,617-byte compiled
`full-live-rack` envelope on Linux and Windows. It verifies the v1 schema,
generation, Rig and profile identities, platform binding, five selected Device
Profiles, control-only readiness, empty/unapplied graph delta, authoring-only
safety flags, and the 72-mapping, 71-target, and 57-ownership counts. Trailing
data and malformed or unsafe documents fail.

This slice decodes metadata only. It does not yet build bounded mapping lookup
tables or recompute the compiler's canonical JSON SHA-256 inside the daemon.
The trusted fingerprint is supplied independently to the loader and compared
with the document field. Output remains unavailable, so no decoded document can
affect MIDI, audio, or graph behavior.

## State And Metrics

Live versioned state records lifecycle, published generation ID, raw definition
fingerprint, output mode, and monotonic start/stop times. The portable persistent
frame is exactly 64 bytes: magic, version, generation, fingerprint,
output-suppressed mode, reserved bytes, and a 64-bit FNV-1a integrity tag. The
tag detects accidental corruption; it is not authentication.

A missing state object starts from the compiled definition. State with the same
fingerprint and a current generation restores. A changed definition fingerprint
or older persisted generation falls back to the compiled generation and
increments a fallback metric. Invalid length, magic, version, reserved fields,
mode, generation, or integrity tag fails closed. Persistence is explicit and
uses the adapter's atomic-replace callback.

Publishing a new caller-owned generation supports an optional
expected-generation precondition. Stale preconditions and non-increasing
generation IDs fail without changing state.

All metrics are unsigned 64-bit saturating counters:

- loop iterations, idle polls, and control waits;
- control, status, invalid, and response counts;
- generation publications and conflicts; and
- state restores, qualified fallbacks, and writes; and
- adapter failures.

Status dispatch returns the current generation. A nonzero stale expected
generation produces result code 5 while keeping the previous and resulting
generation equal. Invalid structured requests produce result code 2.

## Executable Boundary

`music-rigd` is built on Linux and Windows but is deliberately inert. It
supports only `--version` and `--help`; invoking it without a command exits with
code 2. It reports the runtime and storage ABI versions but has no configuration,
definition path, transport, installation, service, or default-start path.

CTest exercises successful idle/request/stop sequencing, status and stale
generation responses, invalid requests, publication conflicts, qualified state
restore/fallback, all 64 single-byte state corruptions, state I/O failures,
definition source/decoder failures, the full compiled-envelope JSON decoder,
invalid lifecycle/configuration, ABI rejection, and daemon inertness. A source
audit rejects allocation and C thread-lock calls from the definition, runtime,
and state core. Existing portability tests continue to reject platform headers
and backend identifiers from the complete core tree.

## Next Runtime Slice

The next slice expands compiled metadata into bounded immutable profile and
mapping lookup tables, freezes the complete read-only and dry-run IPC/CLI
contract, and adds mock Linux/Windows control transports. It remains
output-suppressed and does not replace the protected single-rig deployment.
