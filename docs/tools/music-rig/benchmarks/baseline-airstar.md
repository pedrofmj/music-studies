# Airstar Stable Rig Baseline

Status: **incomplete**

Capture: live-read-only-ssh at 2026-08-10T04:45:45-03:00

The observer is read-only: it creates no remote files, changes no graph
connections, and controls no services.

## Compatibility

| Check | Expected | Observed | Result |
| --- | ---: | ---: | --- |
| Project SHA-256 | protected | 48ea4f777c68ebd39105902739258795edfa96fefd8820b62fb31d4efe329974 | PASS |
| Plugins | 49 | 49 | PASS |
| Unique plugin names | 49 | 49 | PASS |
| Project connections | 111 | 111 | PASS |
| Raw graph links | 117 | 118 | FAIL |
| MIDI graph links | 67 | 68 | FAIL |
| Quantum | 2048 | 2048 | PASS |
| Sample rate | available | 48000 | PASS |
| Stable services | active | 4 | PASS |

## Performance Snapshot

| Metric | Observed |
| --- | ---: |
| PipeWire ERR total | 0 |
| Relevant process CPU | 2.1% |
| Relevant process RSS | 190570496 bytes |
| Stable service memory | 3360202752 bytes |

## Graph Difference

Unexpected live links:
- `PD-CH-1 Gain Map:events-out` -> `smc-pad-gain-verify:input`

Missing protected links:
- None

## Services

| Service | Enabled | Active | PID | Restarts |
| --- | --- | --- | ---: | ---: |
| arturia-main-volume-encoder.service | enabled | active/running | 2067289 | 0 |
| smk25-pad-layers.service | enabled | active/running | 2067297 | 0 |
| pipewire-carla-quantum.service | enabled | active/exited | 0 | 0 |
| pipewire-patchbay-watch.service | enabled | active/running | 1733471 | 0 |
