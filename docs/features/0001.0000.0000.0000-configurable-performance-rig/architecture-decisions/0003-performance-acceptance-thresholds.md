# ADR 0003: Performance Acceptance Thresholds

Status: **Accepted**

Date: 2026-08-10

Feature: `0001.0000.0000.0000`

## Context

The Performance Rig must switch predictably without adding perceptible control
delay, consuming excessive idle resources, or destabilizing audio. The feature
proposal set initial targets, and the portable switch-benchmark contract made
most of them machine-readable. Milestone 0 must decide whether those values are
the acceptance gates before runtime implementation begins.

Selecting a gate is not evidence that an implementation passes it. The current
checked-in switch result is synthetic. The protected Airstar observation also
reports combined existing-process and service resources, not the future
`music-rigd` daemon in isolation, so those values cannot certify the new
runtime.

The benchmark contract previously used its `idle` switch scenario to evaluate
idle CPU. That scenario performs at least 1,000 switches and therefore does not
represent the stated requirement of no incoming events. This decision corrects
that measurement boundary.

## Decision

The initial feature targets are accepted as the portable acceptance ceilings.
Linux and Windows use the same values unless a later reviewed decision changes
the contract with reference-machine evidence.

### Timing Gates

| Behavior | Acceptance gate | Boundary |
| --- | ---: | --- |
| Control-only commit | p95 at or below 20,000,000 ns | Request received to immutable generation published |
| Real-time adoption after commit | maximum at or below one processing period plus 5,000,000 ns | Generation published to first real-time cycle using it |
| Prepared-audio commit | p95 at or below 100,000,000 ns | Request received to audible prepared-graph commit complete |
| MIDI management trigger | p95 at or below 30,000,000 ns | Management event received to the same control-only generation commit |

Every switch scenario contains at least 1,000 attempts. Timing summaries use
successful samples and the nearest-rank percentile method, but any failed
attempt makes the scenario fail. A low percentile cannot hide rejected,
timed-out, or rolled-back requests.

The adoption gate is a maximum, not a percentile. At the protected 2,048-frame,
48 kHz configuration, one period is 42,666,667 ns and the maximum
commit-to-adoption delay is 47,666,667 ns. Control commit and effective
real-time adoption remain separately reported; they are not collapsed into a
more favorable number.

The prepared-audio limit applies only after every engine, asset, port, and
state object is prepared and ready. Plugin discovery, asset loading, engine
creation, and other cold work are outside this interval and remain forbidden
for an unannounced live trigger.

The MIDI trigger limit includes management-event dispatch and the same
transaction used by the CLI. Milestone 5 will add its raw-event timing contract;
the general switch fixture does not claim to measure MIDI receipt.

### Resource Gates

| Resource | Acceptance gate | Boundary |
| --- | ---: | --- |
| Idle daemon CPU | less than 0.5% of one core | At least 60 seconds with zero control requests and zero MIDI events |
| Daemon peak RSS | less than 50,000,000 bytes | No-event observation and every switch scenario |

The CPU and RSS comparisons are exclusive: exactly `0.5%` CPU or exactly
`50,000,000` bytes fails.

The benchmark keeps the `idle`, `normal-performance`, and `high-midi-load`
switch scenarios. Here, `idle` means no competing musical-event load while the
benchmark still requests switches. A separate `idle_observation` is the
authority for no-event CPU and also contributes an RSS gate.

Daemon RSS excludes plugin hosts, Carla, sample data, and prepared engines.
Those values remain separately measured. A single fixed plugin-resource limit
would be misleading because an organ, sampled piano, synthesizer, and a mixer
profile with no engine changes have materially different working sets.
Milestone 6 therefore adds
an explicit warm-resource budget per machine and Rig Profile, deterministic
eviction, a no-swapping requirement during certified performance, and separate
prepared-resource and plugin-host peaks. No prepared campaign passes without
such a configured budget.

Wakeups are recorded but do not yet have a numeric gate. The runtime must use
event-driven waits; Linux and Windows shadow evidence will establish whether a
portable wakeup ceiling is meaningful before one is added.

### Stability Gates

For every scenario, all attributable counters must equal zero:

| Counter | Maximum attributable increase |
| --- | ---: |
| Audio xruns | 0 |
| Audible dropouts | 0 |
| Audio deadline errors | 0 |

Each counter records `before`, `after`, and `attributable`, where
`attributable = after - before`. Existing historical events do not fail a run,
and new events cannot be hidden by reporting only a cumulative total.

An operator-observed click or dropout invalidates a certification run even if
the available platform counter does not increment. Audible-transition evidence
is added with the prepared-audio tests in Milestone 6.

## Rationale

The limits preserve the user-facing targets already reviewed in the feature
proposal while leaving substantial engineering margin between local transport
overhead and the control-commit ceiling. The existing Linux IPC spike is useful
feasibility evidence, but it is not a daemon switch benchmark and does not pass
the feature gate by itself.

The period-relative adoption rule is portable and avoids disguising Airstar's
current 2,048-frame period as control-plane latency. A platform with a smaller
processing period gets a correspondingly tighter adoption bound.

Zero attributable stability failures is intentionally stricter than an
average-rate allowance. A profile switch is an explicit operation under our
control; accepting a known xrun or dropout would make the system unsuitable for
live performance.

The 50 MB daemon ceiling applies only to the switching runtime. Comparing it to
the protected observation's combined Carla, plugin, and service memory would
mix ownership boundaries and weaken the result.

## Evidence And Certification

The machine-readable authority is the version 2
`docs/tools/music-rig/benchmarks/switch-benchmark-contract.json`. Its validator
requires:

- the three ordered switch scenarios;
- at least 1,000 attempts per scenario;
- a separate 60-second zero-event observation;
- consistent CPU, RSS, stability, percentile, and period calculations;
- exact threshold values;
- truthful pass/fail evaluation even for valid failing evidence; and
- retained raw switch samples for real measurements.

Version 2 adds the mandatory no-event observation. No real version 1
measurement exists, so only the synthetic fixture moves to the new shape. The
checked-in fixture proves the contract and validator, not system performance.
Certification evidence is collected later:

- Milestone 3: Linux and Windows output-suppressed shadow idle resources;
- Milestone 4: Linux control-only CLI switching;
- Milestone 5: Linux and Windows MIDI-trigger timing;
- Milestone 6: Linux prepared-audio resource and stability campaigns; and
- Milestone 7: complete Windows switch campaigns.

A campaign must identify its reference machine, platform clock, compiled
definition, benchmark definition, and machine definition by stable
fingerprints. Results for different fingerprints are not silently combined.

## Revision Policy

A result file cannot override these thresholds. Tightening or relaxing a value
requires a reviewed contract change. A relaxation additionally requires:

1. measurement evidence from the affected reference machine;
2. an explanation of the user-visible and audio-stability impact;
3. a superseding architecture decision;
4. updated positive and boundary tests; and
5. preserved prior evidence for comparison.

Machine-specific prepared-resource budgets are not relaxations of the daemon
RSS ceiling because they account for a separate ownership domain.

## Consequences

- Runtime and adapter work receives exact pass/fail boundaries before it can
  shape measurements around an implementation.
- A real no-event window now measures idle CPU instead of a switch workload.
- The high Airstar processing period remains visible in adoption results.
- Cross-platform results share timing and stability semantics.
- Plugin and sample memory cannot be charged to or hidden inside daemon RSS.
- Completion of this ADR closes threshold selection only; no platform is yet
  certified against the thresholds.

No live process, service, audio/MIDI client, or graph connection was changed
while recording or validating this decision.
