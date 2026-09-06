# Music Rig Benchmarks

The [2026-08-11 Airstar Hardware Preset capture](hardware-preset-airstar-2026-08-11.json)
records the exact current SMC-PAD and SMC-PAD Pocket pad assignments, the
Pocket's non-MIDI hardware controls, and matching pre/post subscription
fingerprints. The temporary observers were removed and the operator confirmed
normal drum audio after cleanup.

## Device-Free EQ Parameter Isolation

`run-carla-eq-parameter-isolation.py` extracts only the protected LSP stereo
8-band EQ into a new temporary project and drives it through Carla's native
rack API. It does not start JACK or PipeWire, open an audio device, load the
rest of the rack, connect hardware, or show a plugin UI. The source project,
fixed-buffer option, parameter values, and CC102-109 mappings are preserved.

The runner requires a C compiler, a compatible `libcarla_host-plugin.so`, the
four matching Carla headers (`CarlaDefines.h`, `CarlaNative.h`, `CarlaBackend.h`,
and `CarlaHost.h`), and an explicitly selected LSP LV2 bundle. It downloads and
installs nothing. The initial implementation uses the
[Carla 2.5.8 native-host API](https://github.com/falkTX/Carla/blob/v2.5.8/source/tests/carla-host-plugin.c).
Headers are in that tag's `source/includes` and `source/backend` directories.

```bash
python3 docs/tools/music-rig/benchmarks/run-carla-eq-parameter-isolation.py \
  --project src/audio-software/carla/projects/pedro-live-rack/pedro.uproject \
  --carla-include /tmp/carla-headers \
  --carla-library /usr/lib/carla/libcarla_host-plugin.so \
  --lv2-bundle /tmp/isolated-lv2/lsp-plugins.lv2 \
  --output /tmp/eq-parameter-evidence
```

The output directory must not exist. It retains the extracted project, build
and host logs, per-block CSV, raw measurement JSON, and a report with hashes of
the source, harness, headers, host library, and plugin binary. Expected Carla
load warnings remain in the host log and cannot corrupt the JSON report.
Assertions, sanitizer failures, and file-loading errors reject the run even
when the host returns a successful process status.

Fifteen scenarios compare silence/audio with fixed gains, direct updates of
all gains, MIDI updates at frame zero or spread across the block, and each
individual band. A fixed non-flat-gain control separates steady EQ processing
from parameter-change cost. Every MIDI value from 0 through 127 is calibrated against
Carla's actual mapping before measurement; direct updates replay those exact
values. Each active scenario verifies mapped values and finite, nonzero audio.
The default is 2,048 measured blocks after 128 warmup blocks per scenario,
at 48 kHz and 1,024 frames. Elapsed and calling-thread CPU timings include
direct setters; their CPU cost is also reported separately.

This is a cost-isolation experiment, not a live acceptance test. It omits the
upstream CC scaling intermediaries, full rack load, GUI, device deadlines,
and PipeWire scheduling. Blocks run as fast as possible while requesting
normal realtime plugin behavior. An over-quantum elapsed time is not an
observed JACK/PipeWire xrun. A copied binary on a different host does not
establish Airstar performance parity.

The extraction contract is tested without loading any plugin:

```bash
python3 docs/tools/music-rig/tests/test-carla-eq-parameter-isolation.py
```

The [September 6 centralstar run](carla-eq-parameter-isolation-centralstar-2026-09-06.json)
uses Airstar's exact LSP 1.2.14 binary with local native Carla 2.5.8. It isolates
millisecond-scale gain-change work while fixed non-flat processing remains
in the tens of microseconds. It does not close the live zero-xrun gate.

### Upstream Comparison

`compare-carla-eq-isolation.py` compares two reports from the same harness. It
requires matching sample rate, buffer, scenario, and MIDI-event contracts; it
checks finite audio, mapping parity, a bounded output-energy delta, and that the
candidate never exceeds the diagnostic quantum. The energy check is deliberately
coarse and is not a frequency-response or listening test.

The September 6 comparison built upstream LSP Parametric Equalizer `1.0.40`
locally and ran it through the same native Carla host. The candidate reduced
parameter-change calling-thread CPU by roughly 13-31x across the eight individual
bands and 17-31x for all-band changes. Mapping and finite-audio checks passed;
output-energy deltas reached 1.11%, so physical response and live scheduling
remain unverified. The compact result is
[recorded here](carla-eq-upstream-comparison-centralstar-2026-09-06.json).

The comparison is offline evidence only. Do not replace the protected plugin or
preset from this result. A live test must use a separately reviewed package and
repeat rollback, audible assessment, and the zero-error mixed-load gate.

### Optional Call-Path Profiling

Add `--profile gprofng` to the runner command to collect user-space clock
samples without kernel performance counters. Add `--profile-symbols PATH`
when matching separate debug symbols are available; for GNU build-ID symbols,
use the directory containing the matching `.debug` file. The runner adds
debug/frame-pointer compiler flags, archives used load objects, and retains
`profile.er`, `profile-summary.txt`, and profile-quality metadata in the report.
It follows no child processes and changes no kernel profiling settings.

Collector warnings are retained and mark `quantitative_profile_valid` false.
Do not use warned sample totals or percentages as precise CPU attribution.
Profiles include setup, calibration, warmup, all scenarios, cleanup, and worker
threads. Use the unprofiled run for baseline timing; profiling overhead and
uncontrolled CPU frequency prevent direct speedup comparisons.

The [September 6 symbol-matched investigation](carla-eq-smoothing-profile-centralstar-2026-09-06.json)
confirms the expensive path inside LSP's gain smoothing. The matching
[parameter smoother](https://github.com/lsp-plugins/lsp-plugins-para-equalizer/blob/1.0.21/src/main/plug/para_equalizer.cpp#L1156)
interpolates every filter slot for each audio sample and processes one sample
at a time. When parameters change, the
[equalizer reconfiguration](https://github.com/lsp-plugins/lsp-dsp-units/blob/1.0.19/src/main/filters/Equalizer.cpp#L234)
rebuilds the whole filter bank. A debugger launched against the copied binary
independently captured `Filter::rebuild` below `Equalizer::process(samples=1)`
inside a 1,024-frame LV2 call. This was a stack probe, not a timing run.

The gprofng capture emitted an interval-timer warning, so only its qualitative
call paths are accepted. Matching debug symbols were unpacked locally, with
the plugin build ID verified; no plugin binary or live setting was changed.
The next comparison must preserve smooth transitions and preset response,
and still cannot replace the physical mixed-load zero-error gate.

## Milestone 3 Shadow Closure

[M3-SHADOW-EVIDENCE.md](M3-SHADOW-EVIDENCE.md) explains the consolidated
portable protocol, state, parity, physical resource, and approved live-shadow
proof. Its machine-readable manifest binds all raw records and the compiled
definition by SHA-256. The validator and its mutation self-test run through
CTest on Linux and Windows:

~~~bash
docs/tools/music-rig/benchmarks/validate-m3-shadow-evidence \
  --validate docs/tools/music-rig/benchmarks/m3-shadow-evidence-2026-08-12.json
docs/tools/music-rig/benchmarks/validate-m3-shadow-evidence --self-test
~~~

## Airstar Baseline

The baseline collector observes the protected Airstar setup without changing
services, files, audio links, MIDI links, metadata, or application state.

The remote observer uses only fixed read operations:

- system and software version queries;
- `pw-dump`;
- `pw-metadata -n settings`;
- one `pw-top -b -n 1` sample;
- `systemctl --user show`;
- `ps`; and
- project file hashing and XML parsing.

It returns JSON on standard output and creates no remote file. The local
collector verifies the protected artifact manifest before SSH, compares the
observation with the known-good structure, and writes JSON and Markdown
reports.

Run from the repository root:

~~~bash
docs/tools/music-rig/benchmarks/capture-airstar-baseline
~~~

To inspect the exact remote code without running it:

~~~bash
docs/tools/music-rig/benchmarks/capture-airstar-baseline --print-observer
~~~

An incomplete report is still written for diagnosis, but the command returns a
failure until the project checksum, plugin and connection counts, graph counts,
services, quantum, and sample-rate checks pass.

## Current Rack Startup

[current-rack-startup-2026-08-10.json](current-rack-startup-2026-08-10.json)
and
[current-rack-startup-2026-08-10.md](current-rack-startup-2026-08-10.md)
record the passing operator-controlled startup and musical-acceptance run.
The result binds the planned contract and validation inputs by SHA-256, records
all five controller checks, and confirms zero PipeWire errors, service
restarts, missing protected links, or Patchbay repairs.

## JSON-C Dependency

[json-c-linux.json](json-c-linux.json) records the Linux package, binary,
process-memory, and parse-time measurements for the opt-in dependency probe.
[json-c-windows.json](json-c-windows.json) records the pinned vcpkg package,
hosted build, static archive and license, exact cross-platform fixture, physical
`beanstar` parse and footprint measurements, hash verification, and cleanup.
Both halves of the dependency gate pass.

See [../JSON-PARSING.md](../JSON-PARSING.md) for the decision boundary and
evidence interpretation.

## Windows Reference Evidence

[windows-beanstar-m0.json](windows-beanstar-m0.json) records the selected
physical Windows machine, hosted artifact provenance and hashes, native
generation-adoption result, named-pipe golden-frame round trips, and cleanup
evidence. It is a synthetic portability and IPC result, not audio or MIDI
certification.

[windows-resource-beanstar.json](windows-resource-beanstar.json) records the
hash-verified native short-process and 60-second zero-event resource run. The
short `music-rig --version` process and synthetic event-waiting daemon report
working set, CPU, threads, handles, exit status, and cleanup. The runner opened
no audio or MIDI API. A separate post-run query confirmed that its temporary
directory and processes were absent after the run.

[windows-backend-capabilities.json](windows-backend-capabilities.json) records
the later read-only MIDI service and compatibility mapping, ASIO registration,
Carla/package, Task Scheduler, and upstream candidate snapshot used by the
Windows adapter decision. It enumerated or opened no media endpoint and made no
machine change.

Reproduce the physical resource run only with a reviewed, hash-verified hosted
bundle in a new directory below the current user's Windows TEMP directory. The
wrapper is [run-windows-resource-reference.ps1](run-windows-resource-reference.ps1).
It refuses pre-existing test processes, enforces the physical thresholds, and
removes its temporary directory in a `finally` block. This is synthetic runtime
resource evidence, not audio, MIDI, or production-daemon certification.

## Portable Switch Benchmark Contract

[SWITCH-BENCHMARK-CONTRACT.md](SWITCH-BENCHMARK-CONTRACT.md) defines the shared
Linux and Windows campaign format, exact timing boundaries, required load
scenarios, a separate 60-second zero-event resource observation, accepted
thresholds, stability counters, and raw-evidence rules. The machine-readable
authority is [switch-benchmark-contract.json](switch-benchmark-contract.json).

Validate the contract and its synthetic fixture without touching the live rig:

~~~bash
docs/tools/music-rig/benchmarks/validate-switch-benchmark --check-contract
docs/tools/music-rig/benchmarks/validate-switch-benchmark --self-test
~~~
