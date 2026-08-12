# Device/MIDI Shadow Adapter

The Milestone 3 Device/MIDI adapter observes compiled controller input and
calculates current mapping and behavior decisions while suppressing every
result that could reach a MIDI output. It is opt-in, uninstalled, and separate
from the protected Arturia and SMK-25 services.

## Portable Event Engine

`music_rig_device_midi_shadow` is caller-owned fixed storage. Its current
64-bit Linux size is 40,456 bytes and its compile-time ceiling is 65,536 bytes.
Initialization requires:

- a lock-free immutable-generation slot;
- a validated compiled table image;
- output mode `MUSIC_RIG_OUTPUT_SUPPRESSED`;
- observer ABI version 1; and
- valid behavior assignments for every stable device slot.

Initialization derives one `device.<slot>.midi-input` identity per compiled
Device Profile and initializes the selected reusable behavior state. Any
partial failure clears the object so it cannot be used.

At the beginning of each callback cycle, the engine adopts the newest published
immutable generation. Each MIDI message is parsed into numeric type, channel,
number, value, and edge fields. Mapping dispatch uses the prepared fixed
256-entry numeric index. The event path performs no allocation, locking,
filesystem access, JSON traversal, or string comparison.

Mapping decisions refer directly to the adopted immutable mapping row. The
optional observer callback must be bounded and real-time safe; it is an
observation hook, not an output adapter. Calculated Arturia and SMK-25 MIDI is
reported only as a suppressed event and increments a saturating counter. No
output backend exists in this library.

Metrics cover callback cycles, generation adoptions, input, parsed, mapped,
unmapped, malformed, and suppressed events. Input, mapping, and suppressed
counts are also retained separately for every stable device slot. They are
written by the single callback thread and read only after the host has stopped.

## Linux JACK Host

`music_rig_jack_midi_shadow` owns one JACK client and a fixed array of input
port handles. It opens `music-rigd-shadow` with `JackNoStartServer`, registers
only the stable `.midi-input` identities, and activates one process callback.
It requires lock-free callback/control status atomics and does not discover
physical devices or create links.

The implementation does not reference JACK output-port flags or any connect,
disconnect, MIDI reserve, MIDI write, or MIDI clear function. A source guard
and the static library's unresolved-symbol audit lock this input-only surface.
Registration and activation failures close the client and clear every handle.
Signal shutdown deactivates and closes the client before metrics are read.

The host is built only on Linux when a JACK runtime library is available. The
portable event engine and its tests build on Linux and Windows without JACK.
The Windows MIDI host remains Milestone 7 work.

## Explicit Daemon Command

The optional JSON-enabled Linux build exposes:

```text
music-rigd run-midi-shadow \
  --definition PATH \
  --expected-fingerprint sha256:HEX \
  --output-suppressed
```

The command loads only the named compiled definition, verifies its independently
supplied fingerprint, initializes the immutable generation and current
Hardware Preset behaviors, registers the input-only JACK host, and waits for
`SIGINT` or `SIGTERM`. On clean shutdown it reports input-port, cycle, event,
mapping, and suppression counts.

All three explicit arguments are mandatory. A missing or mismatched definition,
unavailable JACK server, registration failure, output-enabled request, or
invalid table fails before lifecycle readiness. `JackNoStartServer` ensures a
missing backend stays missing. The command selects no installed definition,
state, service, graph, or device path.

The existing `run-shadow --output-suppressed` command and checked-in systemd
unit are unchanged. They still load no definition and open no MIDI, audio,
graph, state, or IPC resource.

## Verification

Portable tests cover MIDI parsing, real mapping dispatch, edge filtering,
malformed input, current Arturia and SMK-25 suppression, generation adoption,
metrics, ABI rejection, output-mode rejection, and fail-closed initialization.
The real 86,617-byte compiled envelope processes one mapping for each of the
five current slots and observes exactly the two calculated behavior outputs as
suppressed.

Linux fake-JACK tests cover stable input registration, callback processing,
registration and activation cleanup, backend shutdown, and absence of any
link/output symbol. An end-to-end process loads the five-slot definition,
reaches lifecycle readiness through an interposed input-only JACK runtime,
handles `SIGTERM`, reports zero output, and creates no per-user path.

Local evidence for this slice:

- GCC default: 51/51;
- GCC JSON/JACK: 55/55;
- focused Clang: 8/8;
- focused ASan/UBSan: 8/8; and
- protected baseline before and after the guarded host attempt: 30/30.

Hosted
[run 31596967924](https://github.com/pedrofmj/music-studies/actions/runs/31596967924)
passes Linux 51/51, Windows 38/38, and Windows JSON 41/41. Both Windows builds
compile and test the portable event engine under strict `/W4 /WX`; the optional
Windows JSON build also exercises all five current device slots.

The completed
[Milestone 3 evidence](benchmarks/M3-SHADOW-EVIDENCE.md) supersedes that guarded
attempt. An explicitly approved Airstar session proved input and mapping for all
five current slots with zero output ports and normal production sound. Physical
Linux and Windows 60-second zero-event measurements pass the CPU/RSS gates.
Every temporary live resource was removed and the protected post-check passed
30/30.
