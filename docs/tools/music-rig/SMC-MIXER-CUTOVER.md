# SMC-Mixer Parity Cutover

This Milestone 4 slice moves only `smc-mixer-main` behind an uninstalled,
explicitly started `music-rigd` relay. It preserves the current
`eight-band-eq` behavior and leaves Arturia, SMK-25, SMC-PAD, SMC-PAD Pocket,
all audio links, all plugins, Carla, and protected services unchanged.

The cutover is not a profile switch. It provisions the profile-independent
stable relay links once. Later control-only profiles reuse those fixed links
and change only immutable mapping/state generations.

## Parity Contract

The portable relay accepts a compiled definition only when
`smc-mixer-main` resolves to:

- Device Profile `eight-band-eq`;
- Hardware Preset `smc-mixer-current-cc`;
- readiness `control-only`;
- exactly eight absolute channel-1 CC mappings, numbers 40 through 47;
- direct transforms and pickup metadata; and
- the eight authored equalizer band targets in order.

Every next immutable generation is fully parity-validated on the control thread
before publication. Callback-cycle adoption is one atomic prepared-pointer
comparison. An unprepared publication drops all prior table references before
failing closed, so acknowledged generations remain reclaimable. For every
accepted event, the callback performs one fixed numeric table lookup and writes
the original three MIDI bytes at the original JACK frame.
It performs no allocation, lock, JSON traversal, filesystem operation, string
comparison, scaling, or graph operation. Unmapped, non-channel-1, non-CC, and
malformed messages produce no output. Saturating metrics separate input,
mapped, emitted, unmapped, malformed, and adapter-failure counts. The JACK host
latches the first callback failure until shutdown so a later quiet cycle cannot
hide it.

The exhaustive portable test compares all 1,024 CC/value combinations. The
fake-JACK test proves exact frame and byte preservation, one input and one
output, output-buffer clearing, write-failure propagation, backend shutdown,
and complete cleanup. Source guards reject allocation, locks, graph discovery,
connection APIs, server startup, platform leakage in the portable layer, and
non-MIDI JACK surfaces.

## Explicit Runtime Boundary

Only the JSON-enabled Linux build exposes:

~~~text
music-rigd run-smc-mixer-relay \
  --definition PATH \
  --expected-fingerprint sha256:HEX \
  --output-enabled \
  --acknowledge-smc-mixer-cutover
~~~

Every argument is mandatory. The command verifies the independently supplied
definition fingerprint before opening JACK, uses `JackNoStartServer`, and
registers only:

~~~text
music-rigd-smc-mixer:device.smc-mixer-main.midi-input
music-rigd-smc-mixer:device.smc-mixer-main.midi-output
~~~

It neither discovers nor changes graph links. No-argument daemon startup,
`run-shadow`, and `run-midi-shadow` retain their existing inert or
output-suppressed behavior. No service or installed activation path is added.

## Reversible Link Transaction

[`deployment/smc-mixer-links`](deployment/smc-mixer-links) owns only the
one-time relay topology. Preview and verification are read-only:

~~~bash
docs/tools/music-rig/deployment/smc-mixer-links --preview
docs/tools/music-rig/deployment/smc-mixer-links --verify-legacy
docs/tools/music-rig/deployment/smc-mixer-links --verify-relay
~~~

`--cutover` requires all eight protected physical-to-CC-scale links and no
relay links. It connects the relay output to the same eight scale inputs,
removes the eight direct links, and connects the physical mixer to the relay
input last. Any failed operation immediately restores the complete legacy
route. A successful relay topology contains zero direct links, one relay input
link, and eight relay output links.

`--rollback` disconnects the relay input first, restores all eight direct
links, removes relay output fan-out, and verifies the legacy topology. It is
idempotent and restores the direct links even after an unexpected daemon exit,
when relay ports and their links have already disappeared.

## First Live Cutover Gate

The first production use requires an explicit operator-approved window. Before
changing ownership:

1. Pass the protected 30-check source verifier and live setup validation.
2. Rehearse the protected production restore with Carla closed, then relaunch
   and validate the stable rack.
3. Stage only a checksummed daemon, compiled definition, link tool, and evidence
   directory under `/tmp`; install and enable nothing.
4. Start the relay without links and verify its exact two-port inventory.
5. Run `--cutover`, exercise all eight faders, and confirm sound and stability.
6. Run `--rollback` while the relay is alive and verify all eight direct links.
7. Stop the relay, remove every temporary artifact, and rerun protected live
   validation.

Stopping the relay before rollback can temporarily leave the SMC-Mixer without
its direct links. The recovery is still `smc-mixer-links --rollback`; full
protected restoration remains the operator fallback, not normal rollback.
