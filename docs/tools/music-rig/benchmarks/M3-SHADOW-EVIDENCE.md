# Milestone 3 Shadow Evidence

The machine-readable
[evidence manifest](m3-shadow-evidence-2026-08-12.json) closes the portable
runtime, CLI, and output-suppressed shadow gate against source commit
`20c069e43837478e73b840e0515b39163257f9d9`. It binds the compiled definition,
Linux and Windows physical resource records, and the approved Airstar live
session by SHA-256.

## Portable Contract Proof

Hosted [run 31622122149](https://github.com/pedrofmj/music-studies/actions/runs/31622122149)
passed Linux 51/51, Windows 38/38, and Windows JSON 43/43 from the same source
commit. The suite covers:

- protocol v2's nine operations, 176-byte requests, 2,592-byte responses,
  golden frames, and 1,000 zero-failure round trips on both transports;
- the 64-byte state frame, every single-byte corruption, qualified restore,
  fingerprint fallback, atomic replacement, and native Linux/Windows storage;
- 5 profile/input bindings, 72 mappings, 71 targets, 57 ownership entries,
  10 stable semantic ports, and an empty control-only graph delta; and
- Arturia and SMK-25 offline differentials plus five-slot numeric dispatch.

Windows uses its current-user named-pipe mock transport and definition-backed
mock MIDI input. This proves the portable protocol, state, transaction, table,
and event-engine contracts. It does not claim a physical Windows MIDI/audio
backend; that certification remains Milestone 7.

## Resource Proof

Both physical reference runs loaded the same compiled current-Rig definition,
used a native blocking wait, received no control or MIDI event for the complete
external 60-second measurement window, and exited cleanly. Neither run opened a
media API, changed a route, installed a service, or touched the live rig.

| Platform | Reference | CPU | Peak observed RSS | Threads | Handle/descriptor peak | Wakeups |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Linux | `centralstar` | 0.000% | 3,162,112 B | 1 | 3 descriptors | 1 |
| Windows | `beanstar` | 0.000% | 6,381,568 B | 4 | 94 handles | 1 |

Both are below the strict `< 0.5%` one-core CPU and `< 50,000,000 B` RSS
limits. The raw records are
[Linux](shadow-idle-linux-2026-08-12.json) and
[Windows](shadow-idle-windows-2026-08-12.json).

## Live Shadow Proof

The explicitly approved Airstar session temporarily duplicated each current
controller input into an uninstalled `music-rigd` process with five inputs and
zero output ports. The normal practice interval ran for 413 seconds and
recorded 2,054 input events, 1,621 mapping decisions, 842 suppressed calculated
events, zero PipeWire errors, normal production audio, and no dropout.

Arturia, SMK-25, SMC-Mixer, and SMC-PAD were proven during that interval. The
Pocket counter was zero, so it was rejected rather than counted. A separate
same-source JACK comparison then captured Pocket note 40 simultaneously in the
system monitor and the Pocket shadow slot: two shadow input events, two mapping
decisions, zero other-slot events, and zero outputs. Its Hardware Preset notes
40, 41, 42, 43, 36, 37, 38, and 39 were also captured independently through
ALSA with note-on and note-off for every pad.

The [raw live record](shadow-live-airstar-2026-08-12.json) retains both rejected
zero-event attempts. After the accepted tests, all experimental links, ports,
processes, and temporary directories were removed. The protected monitor was
preserved; pre/post checks passed 30/30; graph links remained 117, MIDI links
remained 67, the Carla project checksum was identical, and service state and
restart counts were unchanged.

## Validation

Run either command from the repository root. They read checked-in files only
and do not contact or change either reference machine:

~~~bash
docs/tools/music-rig/benchmarks/validate-m3-shadow-evidence \
  --validate docs/tools/music-rig/benchmarks/m3-shadow-evidence-2026-08-12.json
docs/tools/music-rig/benchmarks/validate-m3-shadow-evidence --self-test
~~~

CTest runs both checks on Linux and Windows. The validator recomputes every
referenced checksum and inventory total, enforces the exclusive resource
limits, requires positive evidence for every device slot, and requires exact
post-session restoration. Its self-test proves seven negative mutations are
rejected.
