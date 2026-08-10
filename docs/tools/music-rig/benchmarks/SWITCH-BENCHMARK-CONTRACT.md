# Switch Benchmark Contract

Status: Milestone 0 measurement contract implemented. The checked-in result is
a synthetic contract fixture, not performance evidence.

The machine-readable authority is
[switch-benchmark-contract.json](switch-benchmark-contract.json). Linux and
Windows benchmark launchers must emit the same
`music-studies/switch-benchmark-result/v1` campaign shape and preserve their raw
samples separately.

This tooling is offline. Validation reads JSON files only; it does not start a
service, open an audio or MIDI client, launch Carla, or change a graph.

## Campaign Shape

One result represents the same switch definition under all required scenarios:

1. `idle`
2. `normal-performance`
3. `high-midi-load`

Each scenario contains at least 1,000 switches. Results identify the Rig,
device slot, two or more Device Profiles, platform, reference machine, clock,
compiled definition, benchmark definition, and machine definition with stable
fingerprints.

The scenario set and order are fixed so that reports cannot omit the expensive
condition while still presenting a passing campaign.

## Timing Boundaries

All duration fields are integer nanoseconds.

| Metric | Start | End |
| --- | --- | --- |
| `control_commit_latency_ns` | Request received | Immutable generation published |
| `effective_adoption_latency_ns` | Request received | First real-time cycle using the generation |
| `commit_to_adoption_latency_ns` | Generation published | First real-time cycle using the generation |
| `prepared_audio_commit_latency_ns` | Request received | Audible prepared-graph commit complete |

Control-only and prepared-audio campaigns both report the first three metrics.
Prepared-audio campaigns additionally report audible graph commit. These values
remain separate; an implementation cannot collapse publication and real-time
adoption into one favorable number.

Every summary records `sample_count`, `failure_count`, p50, p95, p99, and
maximum. Percentiles must be ordered and each sample count must match the
campaign's iteration count.

The processing period is the ceiling of:

```text
processing_period_frames * 1,000,000,000 / sample_rate_hz
```

At the protected 2,048-frame, 48 kHz baseline this is 42,666,667 ns. The maximum
generation-publication-to-adoption delay is one period plus 5,000,000 ns, or
47,666,667 ns for that baseline.

`sample_count` includes every attempted switch. Percentiles use successful
samples only and follow the nearest-rank method: sort durations ascending and
select one-based rank `ceil(percentile / 100 * successful_count)`. A campaign
with no successful samples is invalid, and any failure makes its timing gate
fail even when the remaining percentiles are low.

## Clock Boundary

- Linux uses `CLOCK_MONOTONIC_RAW`.
- Windows uses `QueryPerformanceCounter`.
- The result must state that the clock is monotonic and the platform's
  highest-resolution clock, and must record its resolution.

The portable result contract contains no operating-system path, service, audio,
or MIDI API names beyond the clock identity needed to interpret measurements.

## Resource And Stability Accounting

Every scenario records wall time, daemon CPU time, one-core CPU percentage,
wakeups, start and peak daemon RSS, prepared-resource memory, and plugin-host
memory. Daemon memory remains separate from plugins, samples, Carla, and other
prepared engines.

Xruns, audible dropouts, and deadline errors each record cumulative `before`,
`after`, and attributable values. The validator requires:

```text
attributable = after - before
```

This prevents unrelated historical counters from being reported as switch
failures and prevents new failures from being hidden.

## Default Gates

These are the current portable acceptance defaults, pending reference-machine
confirmation:

| Gate | Default |
| --- | ---: |
| Control commit p95 | at most 20,000,000 ns |
| Prepared-audio commit p95 | at most 100,000,000 ns |
| Adoption margin after one processing period | 5,000,000 ns |
| Idle daemon CPU | less than 0.5% of one core |
| Daemon RSS | less than 50,000,000 bytes |
| Attributable xruns | 0 |
| Attributable audio dropouts | 0 |
| Attributable deadline errors | 0 |

Linux and Windows may eventually select different reference-machine budgets,
but any revision must be explicit in a new contract version or reviewed contract
change. Result files cannot silently substitute thresholds.

Schema validity and performance acceptance are separate. A campaign that misses
a gate remains valid evidence when its scenario and campaign evaluations
truthfully report the failure.

## Raw Evidence

A real `measurement` result must reference a portable relative raw-sample path,
sample schema, SHA-256 digest, and exact sample count. Only the checked-in
`contract-fixture` result may omit raw samples, and it labels its summaries as
synthetic.

## Validation

Run all contract checks without interacting with the live rack:

```bash
docs/tools/music-rig/benchmarks/validate-switch-benchmark --check-contract
docs/tools/music-rig/benchmarks/validate-switch-benchmark \
  --validate docs/tools/music-rig/benchmarks/tests/fixtures/switch-benchmark-result-valid.json
docs/tools/music-rig/benchmarks/validate-switch-benchmark --self-test
```

The semantic validator uses only the Python standard library and runs on Linux
and Windows. The daemon and portable C core do not depend on Python.
