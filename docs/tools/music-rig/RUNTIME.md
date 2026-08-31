# Portable Runtime Control Loop

The portable runtime is a platform-neutral, output-suppressed control
dispatcher with Milestone 4 global and device commit transactions. It establishes
the definition, qualified persistent state, metrics, generation, and adapter
contracts that Linux and Windows daemon hosts use. It does not create an IPC
endpoint, bind a device, inspect or change a graph, contact a plugin host,
install a service, or start by default. The Linux host resolves per-user
locations and provides an explicit output-suppressed lifecycle without using
those paths for I/O.

## Ownership And Storage

`music_rig_runtime`, decoded definition metadata and tables, document
workspaces, and every published immutable `music_rig_generation` use
caller-owned storage. Runtime initialization copies the first generation,
32-byte compiled-definition fingerprint, and versioned adapter table; it
performs no allocation.

One control thread owns runtime initialization, `music_rig_runtime_run`, state,
and metrics. Platform callbacks execute synchronously on that thread. A future
real-time callback reads only the separately published immutable generation
pointer through the existing lock-free generation slot. State and metric
accessors return live read-only views and are not cross-thread snapshots.

The generation slot has a fixed eight-entry retirement ring. Publication
retires the previous pointer; it never frees or prematurely reuses generation
storage. The runtime embeds nine commit-generation records in its caller-owned
top-level storage and reserves two free records plus two retirement entries
before a durable commit so persistence rollback remains possible. The control
thread may reclaim entries only after the real-time adopter has published a
newer adopted pointer. Internal initial and commit records are reclaimed
silently; external generation pointers are returned to their caller. A full
ring returns invalid state without publishing, providing bounded backpressure
instead of unbounded retention.

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
fingerprint, an active Rig Profile ID, output-suppressed mode, runtime adapter
ABI version 6, storage adapter ABI version 1, and a prepared immutable table
image with a valid stable device-port catalogue. It reads qualified state
before publishing the initial generation. `run` may be called exactly once. A
normal stop closes the control adapter and records the monotonic stop time.
Start, poll, wait, response, stop, state-read, or state-replace failures remain
explicit.

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

Storage callbacks receive logical object identifiers, not paths. The
`music_rig_file_storage` host adapter accepts caller-owned explicit UTF-8
definition and state paths. It reads both logical objects but permits atomic
replacement only for runtime state. It never creates parent directories or
selects XDG, Known Folder, installed, or protected-rig locations.

Linux replacement writes and flushes a same-directory private temporary file
before `rename`. Windows converts UTF-8 paths to native UTF-16, writes and flushes
a uniquely created same-directory file, and commits it with replacement and
write-through flags. Contract tests use build-directory state files and remove
them after each run. The portable core still contains no platform handles,
paths, or backend names. IPC authentication, device I/O, graphs, plugin hosts,
service installation, and production activation remain host work.
The adapter owns no worker thread, polling loop, cache, or heap allocation; all
file work is synchronous on the control path and prohibited from real-time
callbacks.

## Linux Host Boundary

The ABI-versioned Linux resolver chooses these logical locations without
calling filesystem APIs or creating a directory:

```text
$XDG_CONFIG_HOME/music-rig/config.json
$XDG_CACHE_HOME/music-rig/compiled
$XDG_STATE_HOME/music-rig/active.state
$XDG_STATE_HOME/music-rig/device-state
$XDG_RUNTIME_DIR/music-rig/control.sock
```

Unset or relative configuration, cache, and state roots fall back below an
absolute `HOME` as required by the XDG conventions. `XDG_RUNTIME_DIR` must be
present and absolute because it has no safe persistent fallback. Resolution is
bounded to 4,096-byte buffers and fails closed on invalid or oversized input.
The explicit Linux daemon host creates the per-user runtime directory and
binds an authenticated filesystem `SOCK_SEQPACKET` endpoint at this path. The
client and response paths use bounded two-second I/O waits; malformed or
unauthorized peers are closed without terminating the daemon. Startup rejects
an active same-user socket and removes only a stale socket.

The portable diagnostic dispatcher owns fixed control-thread storage, validates
bounded printable codes and messages, applies a fixed-window burst limit, and
keeps saturating attempted, emitted, suppressed, and sink-failure counters. It
allocates no memory, starts no thread, takes no lock, and is prohibited from
real-time callbacks. The Linux sink formats one bounded structured line and
writes it synchronously to an injected descriptor. The service contract uses
stderr with `StandardError=journal`.

The explicit Linux lifecycle installs only `SIGINT` and `SIGTERM` handlers,
emits start/stop records through that limiter, blocks in a race-free
`sigsuspend` wait, restores the previous handlers and signal mask, and exits. It
opens no definition, state, IPC, MIDI, audio, graph, or plugin-host resource.
The checked-in
`packaging/linux/music-rigd.service` is not installed by this project, calls
only the read-only path preflight and output-suppressed lifecycle, and remains
guarded by `%E/music-rig/shadow-enabled`.

## Compiled Definition Loading

`music_rig_definition_load` reads a compiled document through the storage
adapter into a caller-owned bounded workspace, invokes a decoder adapter, checks
the portable metadata and table contracts, compares the recorded fingerprint
with a trusted expected fingerprint, and prepares a caller-owned immutable
table image. A separate call initializes a generation whose mapping pointer
refers directly to that table image.

Compiled-table ABI version 1 has explicit maximums: 16 Device Profiles and
input bindings, 4 endpoints per input, 128 mappings, a 256-entry mapping
dispatch index, 128 target bindings, 128 ownership entries, and 8 owners per
entry. Oversize, incomplete, duplicate, mismatched, or unprepared tables fail
closed. The complete table image is 451,032 bytes in the current 64-bit
GCC/Clang builds; the offline validator reports `sizeof` for each platform.
It is static caller-owned storage, not resident JSON or owned heap state.

Mapping preparation builds a fixed open-addressed index from Device Profile
index, MIDI event type, channel, and number. Event lookup compares compact
numeric fields and returns the immutable mapping row without allocation, locks,
JSON traversal, or string comparison. Profile and target discovery use sorted
bounded tables; ownership discovery is bounded by its fixed maximum. The
no-allocation/no-lock source audit covers table preparation and lookup.

The optional `json-c` adapter now decodes the checked-in 86,617-byte compiled
`full-live-rack` envelope on Linux and Windows. It verifies the v1 schema,
generation, Rig and profile identities, platform binding, five selected Device
Profiles and input bindings, control-only readiness, empty/unapplied graph
delta, authoring-only safety flags, 72 mapping rows plus every compiler
`mapping_index` entry, 71 target bindings, and 57 ownership entries. Profile,
Hardware Preset, dispatch-key, target, ownership-key, and owner relationships
are cross-checked. Trailing data, a misdirected compiler index, and malformed,
unsafe, or over-capacity documents fail.

An opt-in JSON-enabled daemon build exposes one explicit offline command:

```text
music-rigd validate-definition --definition PATH \
  --expected-fingerprint sha256:HEX
```

It reads only the named document through the native file adapter, validates the
metadata, tables, and trusted fingerprint, initializes a caller-owned immutable
generation, reports its bounded inventory and table-storage size, and exits. It
has no state path and cannot write, activate, connect, or publish output.

The daemon still does not recompute the compiler's canonical JSON SHA-256. The
trusted fingerprint is supplied independently to the loader and compared with
the document field. Output remains unavailable, so no decoded table can affect
MIDI, audio, or graph behavior.

## Stable Device-Slot Ports

Every prepared table image derives exactly two portable identities per stable
device slot, ordered by slot and direction:

```text
device.<slot>.midi-input
device.<slot>.midi-output
```

For the current five-device Rig this is a fixed ten-port catalogue. Port IDs
depend only on the stable slot, never on a Device Profile, physical locator,
backend ID, or operating system. Profile-only generations therefore keep the
same catalogue. Runtime publication fully validates the next table image and
rejects a changed port catalogue before publishing the generation pointer.

The portable catalogue remains semantic and backend-neutral. The opt-in Linux
JACK shadow host now registers only the five `.midi-input` identities for its
process lifetime. It does not register output ports, discover devices, create
links, or emit events. WinMM and other host bindings remain later work.

## State And Metrics

Live versioned state records lifecycle, published generation ID, raw definition
fingerprint, output mode, and monotonic start/stop times. Persistent state v3 is
exactly 800 bytes: the v2 fields plus five fixed 130-byte device-override
entries, reserved bytes, and a final 64-bit FNV-1a integrity tag. The tag
detects accidental corruption; it is not authentication. The decoder remains
backward-compatible with the 128-byte v2 and 64-byte v1 frames; older frames
restore with no device overrides.

A missing state object starts from the compiled definition. State with the same
fingerprint, a current generation, and an available base or prepared Rig Profile
restores that immutable table image. A changed fingerprint, older generation,
or unavailable persisted profile falls back to the compiled base generation and
increments a fallback metric. Invalid length, magic, version, reserved fields,
mode, generation, profile ID, or integrity tag fails closed. Persistence uses
the adapter's atomic-replace callback.

Publishing a new caller-owned generation supports an optional
expected-generation precondition. Stale preconditions, non-increasing
generation IDs, retirement-ring saturation, invalid tables, and changed stable
port catalogues fail without changing state. Control inspection follows the
most recently published generation's immutable table pointer.

All metrics are unsigned 64-bit saturating counters:

- loop iterations, idle polls, and control waits;
- control, status, list, validation, dry-run, unsupported, invalid, and response
  counts;
- generation publications, conflicts, reclamations, and backpressure;
- global commit requests, successes, rollbacks, and rollback failures;
- stable port-identity conflicts;
- state restores, qualified fallbacks, and writes; and
- adapter failures.

The Device/MIDI shadow adapter separately records input, mapping, and
suppressed calculated-event counters for every stable device slot. This makes a
five-device aggregate insufficient by itself: each slot must have its own
positive evidence before parity is accepted.

Protocol v2 dispatch implements status, filtered profile listing, active
validation, and dry-run prepare/switch/reset/reload planning over immutable
table images. Runtime initialization accepts at most 16 caller-owned prepared
definitions, fully validates each definition and table, rejects duplicate Rig
Profile IDs, and requires its stable device-slot port catalogue to match the
base generation. Profile listing and global/device dry-runs can inspect those
candidates without publishing.

The runtime accepts non-dry-run global and per-device switches in
output-suppressed mode and exercises the same transaction phases in
output-enabled mode through its caller-owned backend adapter. It
plans the target through the same dispatcher, preserves the configured base
profile as a switch-back target, publishes a bounded internal generation, and
persists active profile plus generation. Persistence failure republishes the
previous mapping and rewrites the rolled-back state; both successful and failed
rollback writes are explicit in the response and metrics. Device switches
compose one prepared profile into the active table while preserving stable
ports, and reset restores the configured base profile. A stale expected
generation produces result code 5 before publication.
Selecting the device's already active profile is an idempotent success: it does
not publish a generation or create a redundant override.

The output-enabled transaction contract is defined in
[`OUTPUT-TRANSACTION.md`](OUTPUT-TRANSACTION.md). It separates preparation,
publication, confirmation, state persistence, and rollback; rollback always
uses a new monotonically increasing generation and fails closed when backend or
durable state cannot be restored.

## Executable Boundary

`music-rigd` is built on Linux and Windows and remains inert without an
explicit command. Every build supports `--version` and `--help`; invoking it
without a command exits with code 2. Linux adds the read-only `resolve-paths
--check-only` preflight and the explicit `run-shadow --output-suppressed`
lifecycle described above. The opt-in JSON build also supports the explicit
read-only definition validation command. A Linux build with a JACK runtime
additionally supports `run-midi-shadow` only with a named definition, expected
fingerprint, and `--output-suppressed`. Only the JSON-enabled Linux JACK build
also exposes `run-smc-mixer-relay`, requiring a named definition, independently
supplied fingerprint, `--output-enabled`, and
`--acknowledge-smc-mixer-cutover`. That command registers one exact
`smc-mixer-main` input and eight fader-specific outputs but cannot discover or
change links; the separate reversible transaction owns the topology. It reports the runtime,
storage, diagnostic, compiled, and applicable JACK host ABIs. The Linux
SMC-MIX JACK host uses fixed-storage per-cycle latest-value coalescing and orders
retained MIDI events by frame. It reports coalesced intermediate control updates.
No build has a configured IPC
transport, installation target, or default-start path, and ordinary builds
contain no output-capable command.

`music-rig` now has a portable parser, transport interface, and human/JSON
renderer for `status`, `profiles list`, `validate`, explicit global/device
prepare and switch dry-runs, a commit-capable global switch, and device-override
reset dry-runs. Prepared profiles render as available rather than active. The
executable has no configured production transport. It fails closed with adapter
result 4 and sends nothing, including for a valid global commit command.

CTest exercises successful idle/request/stop sequencing, read-only and dry-run
responses, filtered inventories, cold warnings, stale generations, unsafe
switch rejection, publication conflicts, qualified state
restore/fallback, all 128 single-byte state corruptions, v1 compatibility,
state I/O failures, definition source/decoder failures, the full
compiled-envelope JSON decoder, native temporary-file definition reads and
atomic state replacements, explicit daemon validation and fingerprint mismatch,
invalid lifecycle/configuration, ABI rejection, bounded generation reuse, stable
port identity, CLI fail-closed behavior, daemon inertness, XDG resolution without
writes, bounded journal records, rate limiting, clean Linux signal shutdown, and
the guarded uninstalled user-unit contract. Linux and Windows mock transports
carry 1,000 mixed v2 requests through the real table dispatcher.
A source audit rejects allocation and C thread-lock calls from the compiled
tables, control dispatcher, definition, device-port catalogue, diagnostics,
runtime, and state core.
Existing portability tests continue to reject platform headers and backend
identifiers from the complete core tree.

## Reusable Current Behaviors

The separate `music_rig_current_behaviors` library reproduces the protected
Arturia relative-volume, mute, connection-replay, and audio-gate transitions and
the protected SMK-25 pad, knob, layer-latch, Play, Stop, transport,
connection-replay, passthrough, and state-recall transitions. Configuration and
mutable state are fixed-size and caller-owned. The library opens no resource and
has no platform, backend, thread, allocation, lock, filesystem, or JSON
dependency.

The event path contains no string comparison or unbounded work. Arturia work is
constant per MIDI event or audio frame; SMK-25 work is bounded by the fixed
eight-layer and 128-note capacities. Current 64-bit Linux sizes are 24-byte and
2,352-byte behavior states, with compile-time ceilings of 64 and 4,096 bytes.
Versioned logical snapshots are in-memory values, not a serialized persistent
ABI.

Portable unit and source-boundary tests run on Linux and Windows. Linux-only
differential fixtures compile the unchanged protected sources offline and
compare emitted decisions and complete relevant state, including legacy text
recall. See [BEHAVIORS.md](BEHAVIORS.md) for the full contract.

## Device/MIDI Shadow Execution

The separate fixed-storage event engine owns per-slot parser and behavior state,
adopts immutable table generations once per callback cycle, and resolves MIDI
through the numeric dispatch index. Current Arturia and SMK-25 calculated
messages terminate at a suppression observer by default. An explicit output
observer enables the separate Linux JACK output host described below.

The Linux JACK shadow host opens with no-server-start, registers only stable
input ports, and contains no connect or output API. The separate Linux output
host registers paired stable ports and emits only through its explicit output
observer; it never changes links. Fake-backend and complete five-device
definition tests exercise the shadow lifecycle without a live graph. See
[DEVICE-MIDI-SHADOW.md](DEVICE-MIDI-SHADOW.md) for the command, ownership,
hot-path, metric, failure, and verification contracts. See
[DEVICE-MIDI-OUTPUT.md](DEVICE-MIDI-OUTPUT.md) for the output-host
initialization and adoption boundary.

## Milestone 3 Evidence

The consolidated [evidence](benchmarks/M3-SHADOW-EVIDENCE.md) proves protocol,
state, compiled parity, and 60-second idle resource behavior on Linux and
Windows. The approved Airstar input-only session proved all five current slots,
normal production sound, zero output, zero attributable errors, and exact
cleanup with a protected 30/30 post-check. Windows remains a portable
mock/input-host proof until the selected Milestone 7 backend is implemented.

## Next Runtime Slice

Milestone 4 starts with independent SMC-Mixer parity validation before any
input ownership cutover. The protected deployment remains authoritative until
that separate cutover gate has a rehearsed rollback and explicit approval.
